/**
 * @file
 * Stack-internal timers implementation.
 * This file includes timer callbacks for stack-internal timers as well as
 * functions to set up or stop timers and check for expired timers.
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *         Simon Goldschmidt
 *
 */

#include "lwip/opt.h"

#include "lwip/timers.h"
#include "lwip/tcp_impl.h"

#if LWIP_TIMERS

#include "lwip/def.h"
#include "lwip/memp.h"
#include "lwip/tcpip.h"

#include "lwip/ip_frag.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/igmp.h"
#include "lwip/dns.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"


/** The one and only timeout list */
static struct sys_timeo *next_timeout;
#if NO_SYS
static u32_t timeouts_last_time;
#endif /* NO_SYS */

#if LWIP_TCP
/** global variable that shows if the tcp timer is currently scheduled or not */
static int tcpip_tcp_timer_active;

/**
 * Timer callback function that calls tcp_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
tcpip_tcp_timer(void *arg)
{
  LWIP_UNUSED_ARG(arg);

  /* call TCP timer handler */
  tcp_tmr();
  /* timer still needed? */
  if (tcp_active_pcbs || tcp_tw_pcbs) {
    /* restart timer */
    sys_timeout(TCP_TMR_INTERVAL, tcpip_tcp_timer, NULL);
  } else {
    /* disable timer */
    tcpip_tcp_timer_active = 0;
  }
}

/**
 * Called from TCP_REG when registering a new PCB:
 * the reason is to have the TCP timer only running when
 * there are active (or time-wait) PCBs.
 */
void
tcp_timer_needed(void)
{
  /* timer is off but needed again? */
  if (!tcpip_tcp_timer_active && (tcp_active_pcbs || tcp_tw_pcbs)) {
    /* enable and start timer */
    tcpip_tcp_timer_active = 1;
    sys_timeout(TCP_TMR_INTERVAL, tcpip_tcp_timer, NULL);
  }
}
#endif /* LWIP_TCP */

#if IP_REASSEMBLY
/**
 * Timer callback function that calls ip_reass_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
ip_reass_timer(void *arg)
{
  LWIP_UNUSED_ARG(arg);
  LWIP_DEBUGF(TIMERS_DEBUG, ("tcpip: ip_reass_tmr()\n"));
  ip_reass_tmr();
  sys_timeout(IP_TMR_INTERVAL, ip_reass_timer, NULL);
}
#endif /* IP_REASSEMBLY */

#if LWIP_ARP
/**
 * Timer callback function that calls etharp_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
arp_timer(void *arg)
{
  LWIP_UNUSED_ARG(arg);
  LWIP_DEBUGF(TIMERS_DEBUG, ("tcpip: etharp_tmr()\n"));
  etharp_tmr();
  sys_timeout(ARP_TMR_INTERVAL, arp_timer, NULL);
}
#endif /* LWIP_ARP */

#if LWIP_DHCP
/**
 * Timer callback function that calls dhcp_coarse_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
dhcp_timer_coarse(void *arg)
{
  LWIP_UNUSED_ARG(arg);
  LWIP_DEBUGF(TIMERS_DEBUG, ("tcpip: dhcp_coarse_tmr()\n"));
  dhcp_coarse_tmr();
  sys_timeout(DHCP_COARSE_TIMER_MSECS, dhcp_timer_coarse, NULL);
}

/**
 * Timer callback function that calls dhcp_fine_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
dhcp_timer_fine(void *arg)
{
  LWIP_UNUSED_ARG(arg);
  LWIP_DEBUGF(TIMERS_DEBUG, ("tcpip: dhcp_fine_tmr()\n"));
  dhcp_fine_tmr();
  sys_timeout(DHCP_FINE_TIMER_MSECS, dhcp_timer_fine, NULL);
}
#endif /* LWIP_DHCP */

#if LWIP_AUTOIP
/**
 * Timer callback function that calls autoip_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
autoip_timer(void *arg)
{
  LWIP_UNUSED_ARG(arg);
  LWIP_DEBUGF(TIMERS_DEBUG, ("tcpip: autoip_tmr()\n"));
  autoip_tmr();
  sys_timeout(AUTOIP_TMR_INTERVAL, autoip_timer, NULL);
}
#endif /* LWIP_AUTOIP */

#if LWIP_IGMP
/**
 * Timer callback function that calls igmp_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
igmp_timer(void *arg)
{
  LWIP_UNUSED_ARG(arg);
  LWIP_DEBUGF(TIMERS_DEBUG, ("tcpip: igmp_tmr()\n"));
  igmp_tmr();
  sys_timeout(IGMP_TMR_INTERVAL, igmp_timer, NULL);
}
#endif /* LWIP_IGMP */

#if LWIP_DNS
/**
 * Timer callback function that calls dns_tmr() and reschedules itself.
 *
 * @param arg unused argument
 */
static void
dns_timer(void *arg)
{
  LWIP_UNUSED_ARG(arg);
  LWIP_DEBUGF(TIMERS_DEBUG, ("tcpip: dns_tmr()\n"));
  dns_tmr();
  sys_timeout(DNS_TMR_INTERVAL, dns_timer, NULL);
}
#endif /* LWIP_DNS */

/** Initialize this module */
void sys_timeouts_init(void)
{
#if IP_REASSEMBLY
  sys_timeout(IP_TMR_INTERVAL, ip_reass_timer, NULL);
#endif /* IP_REASSEMBLY */
#if LWIP_ARP
  sys_timeout(ARP_TMR_INTERVAL, arp_timer, NULL);
#endif /* LWIP_ARP */
#if LWIP_DHCP
  sys_timeout(DHCP_COARSE_TIMER_MSECS, dhcp_timer_coarse, NULL);
  sys_timeout(DHCP_FINE_TIMER_MSECS, dhcp_timer_fine, NULL);
#endif /* LWIP_DHCP */
#if LWIP_AUTOIP
  sys_timeout(AUTOIP_TMR_INTERVAL, autoip_timer, NULL);
#endif /* LWIP_AUTOIP */
#if LWIP_IGMP
  sys_timeout(IGMP_TMR_INTERVAL, igmp_timer, NULL);
#endif /* LWIP_IGMP */
#if LWIP_DNS
  sys_timeout(DNS_TMR_INTERVAL, dns_timer, NULL);
#endif /* LWIP_DNS */

#if NO_SYS
  /* Initialise timestamp for sys_check_timeouts */
  timeouts_last_time = sys_now();
#endif
}

/**
 * Create a one-shot timer (aka timeout). Timeouts are processed in the
 * following cases:
 * - while waiting for a message using sys_timeouts_mbox_fetch()
 * - by calling sys_check_timeouts() (NO_SYS==1 only)
 * 向内核注册一个定时事件
 * @param msecs time in milliseconds after that the timer should expire 事件的定时时间
 * @param handler callback function to call when msecs have elapsed  处理事件的函数
 * @param arg argument to pass to the callback function	传向h的参数
 * 插入规则举例：a(200)->b(200)->c(50)->d(100) 我们要插入e(350) a(200)->e(150)->b(50)->c(50)->d(100)
 */
#if LWIP_DEBUG_TIMERNAMES
void
sys_timeout_debug(u32_t msecs, sys_timeout_handler handler, void *arg, const char* handler_name)
#else /* LWIP_DEBUG_TIMERNAMES */
void
sys_timeout(u32_t msecs, sys_timeout_handler handler, void *arg)
#endif /* LWIP_DEBUG_TIMERNAMES */
{
  struct sys_timeo *timeout, *t;

  timeout = (struct sys_timeo *)memp_malloc(MEMP_SYS_TIMEOUT);	//为新事件申请内存池空间
  if (timeout == NULL) { //申请失败直接退出
    LWIP_ASSERT("sys_timeout: timeout != NULL, pool MEMP_SYS_TIMEOUT is empty", timeout != NULL);
    return;
  }
  timeout->next = NULL;	//设置sys_timo的四个字段
  timeout->h = handler;
  timeout->arg = arg;
  timeout->time = msecs;
#if LWIP_DEBUG_TIMERNAMES
  timeout->handler_name = handler_name;
  LWIP_DEBUGF(TIMERS_DEBUG, ("sys_timeout: %p msecs=%"U32_F" handler=%s arg=%p\n",
    (void *)timeout, msecs, handler_name, (void *)arg));
#endif /* LWIP_DEBUG_TIMERNAMES */

  if (next_timeout == NULL) { //链表上还没有任何定时事件
    next_timeout = timeout;	//新定时事件为第一个定时事件 直接返回
    return;
  }
  /* 需要将定时事件插入到定时列表中*/
  if (next_timeout->time > msecs) {	//新事件的定时事件比第一个事件的定时时间短
    next_timeout->time -= msecs;	//调整第一个定时事件的定时事件
    timeout->next = next_timeout;	//新事件成为定时链表上的第一个事件
    next_timeout = timeout;	
  } else {	//否则将新事件插入到链表中的某个位置
    for(t = next_timeout; t != NULL; t = t->next) {
      timeout->time -= t->time;		//调整新事件的定时时间 因为前面的所有事件都执行完毕才等到它执行
      if (t->next == NULL || t->next->time > timeout->time) { //满足插入条件
        if (t->next != NULL) {
          t->next->time -= timeout->time;	//调整插入位置的等待时间
        }
        timeout->next = t->next;	//插入t处
        t->next = timeout;
        break;	
      } 
    } //for
  } //else
}

/**
 * Go through timeout list (for this task only) and remove the first matching
 * entry, even though the timeout has not triggered yet.
 * 从定时链表中删除一个定时事件，a和h为唯一区分各个定时事件的标志
 * @note This function only works as expected if there is only one timeout
 * calling 'handler' in the list of timeouts.
 *
 * @param handler callback function that would be called by the timeout
 * @param arg callback argument that would be passed to handler
*/
void
sys_untimeout(sys_timeout_handler handler, void *arg)
{
  struct sys_timeo *prev_t, *t;

  if (next_timeout == NULL) {	//如果是空的直接返回
    return;
  }

  for (t = next_timeout, prev_t = NULL; t != NULL; prev_t = t, t = t->next) {
    if ((t->h == handler) && (t->arg == arg)) {	//如果成功匹配
      /* We have a match */
      /* Unlink from previous in list */
      if (prev_t == NULL) {	//如果是头节点
        next_timeout = t->next;	//直接删除
      } else {	//否则是中间节点
        prev_t->next = t->next;	//删除中间节点
      }
      /* If not the last one, add time of this one back to next */
      if (t->next != NULL) {	//如果不是最后一个节点
        t->next->time += t->time;	//将它后面的定时事件的定时时间加上要被删除的时间
      }
      memp_free(MEMP_SYS_TIMEOUT, t);	//释放
      return;
    }
  }
  return;
}

#if NO_SYS

/** Handle timeouts for NO_SYS==1 (i.e. without using
 * tcpip_thread/sys_timeouts_mbox_fetch(). Uses sys_now() to call timeout
 * handler functions when timeouts expire.
 *
 * Must be called periodically from your main loop.
 */
void
sys_check_timeouts(void)
{
  if (next_timeout) {
    struct sys_timeo *tmptimeout;
    u32_t diff;
    sys_timeout_handler handler;
    void *arg;
    u8_t had_one;
    u32_t now;

    now = sys_now();
    /* this cares for wraparounds */
    diff = now - timeouts_last_time;
    do
    {
#if PBUF_POOL_FREE_OOSEQ
      PBUF_CHECK_FREE_OOSEQ();
#endif /* PBUF_POOL_FREE_OOSEQ */
      had_one = 0;
      tmptimeout = next_timeout;
      if (tmptimeout && (tmptimeout->time <= diff)) {
        /* timeout has expired */
        had_one = 1;
        timeouts_last_time = now;
        diff -= tmptimeout->time;
        next_timeout = tmptimeout->next;
        handler = tmptimeout->h;
        arg = tmptimeout->arg;
#if LWIP_DEBUG_TIMERNAMES
        if (handler != NULL) {
          LWIP_DEBUGF(TIMERS_DEBUG, ("sct calling h=%s arg=%p\n",
            tmptimeout->handler_name, arg));
        }
#endif /* LWIP_DEBUG_TIMERNAMES */
        memp_free(MEMP_SYS_TIMEOUT, tmptimeout);
        if (handler != NULL) {
          handler(arg);
        }
      }
    /* repeat until all expired timers have been called */
    }while(had_one);
  }
}

/** Set back the timestamp of the last call to sys_check_timeouts()
 * This is necessary if sys_check_timeouts() hasn't been called for a long
 * time (e.g. while saving energy) to prevent all timer functions of that
 * period being called.
 */
void
sys_restart_timeouts(void)
{
  timeouts_last_time = sys_now();
}

#else /* NO_SYS */

/**
 * Wait (forever) for a message to arrive in an mbox.
 * While waiting, timeouts are processed.
 * 等待 直到在邮箱中收到消息 在等待过程中 定时事件被执行
 * @param mbox the mbox to fetch the message from
 * @param msg the place to store the message
 */
void
sys_timeouts_mbox_fetch(sys_mbox_t *mbox, void **msg)
{
  u32_t time_needed;
  struct sys_timeo *tmptimeout;	//定时结构指针
  sys_timeout_handler handler;	//定时事件函数指针
  void *arg;	//定时事件函数参数

 again:
  if (!next_timeout) {	//定时事件链表上的定时事件为空
    time_needed = sys_arch_mbox_fetch(mbox, msg, 0);	//则邮箱一直被阻塞
  } else {	//否则处理链表上的第一个定时事件
    if (next_timeout->time > 0) {
		/* 如果定时时间大于0 则以响应时长阻塞邮箱 并用time_needed记录等待的时间 */
      time_needed = sys_arch_mbox_fetch(mbox, msg, next_timeout->time);	//阻塞等待next_timeout->time的时间
    } else {	//如果定时时长为0，则将time_needed设置为SYS_ARCHTIMEOUT
      time_needed = SYS_ARCH_TIMEOUT; //最后time_needed还是会被赋值为SYS_ARCH_TIMEOUT
    }
	/* 等待了第一个事件需要的时长 但还未收到消息 则处理第一个定时事件 */
    if (time_needed == SYS_ARCH_TIMEOUT) {
      /* If time == SYS_ARCH_TIMEOUT, a timeout occured before a message
         could be fetched. We should now call the timeout handler and
         deallocate the memory allocated for the timeout. */
      tmptimeout = next_timeout;	//获取第一个定时事件
      next_timeout = tmptimeout->next;	//从定时链表上删除第一个定时事件
      handler = tmptimeout->h;	//获取定时事件处理函数
      arg = tmptimeout->arg;	//获取定时事件处理函数需要传入的参数
#if LWIP_DEBUG_TIMERNAMES
      if (handler != NULL) {	
        LWIP_DEBUGF(TIMERS_DEBUG, ("stmf calling h=%s arg=%p\n",
          tmptimeout->handler_name, arg));
      }
#endif /* LWIP_DEBUG_TIMERNAMES */
      memp_free(MEMP_SYS_TIMEOUT, tmptimeout);
      if (handler != NULL) {	//如果定时事件处理函数被定义
        /* For LWIP_TCPIP_CORE_LOCKING, lock the core before calling the
           timeout handler function. */
        LOCK_TCPIP_CORE();
        handler(arg);	//执行之
        UNLOCK_TCPIP_CORE();
      }
      LWIP_TCPIP_THREAD_ALIVE();

      /* We try again to fetch a message from the mbox. */
      goto again;	//返回again继续阻塞执行定时事件
    } else { //调整等待时间后退出 让tcpip_thread处理接收到的消息
      /* If time != SYS_ARCH_TIMEOUT, a message was received before the timeout
         occured. The time variable is set to the number of
         milliseconds we waited for the message. */
      /* 如果在指定时间内成功获取了一条消息，time_needed记录了等待消息所消耗的时间*/
      if (time_needed < next_timeout->time) {
        next_timeout->time -= time_needed;  //调整第一个事件的等待时间
      } else {
        next_timeout->time = 0;
      }
    }
  }
}

#endif /* NO_SYS */

#else /* LWIP_TIMERS */
/* Satisfy the TCP code which calls this function */
void
tcp_timer_needed(void)
{
}
#endif /* LWIP_TIMERS */
