#include <linux/module.h>
#include <linux/kernel.h>
#include <net/tcp.h>
#include <linux/inet.h>

static int tcp_dupack_count(const struct tcp_sock *tp)
{
    return tp->retrans_out;
}

static bool tcp_in_recovery(const struct tcp_sock *tp)
{
    return tp->snd_cwnd < tp->snd_ssthresh;
}

void tcp_enter_recovery(struct sock *sk, bool ece_ack)
{
    struct tcp_sock *tp = tcp_sk(sk);
    tp->high_seq = tp->snd_nxt;
    tp->snd_cwnd = tp->snd_ssthresh;
}

static void tcp_end_recovery(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);
    tp->snd_cwnd = tp->prior_cwnd;
}

void tcp_reno_init(struct sock *sk)
{
    struct tcp_sock *tp = tcp_sk(sk);
    tp->snd_ssthresh = TCP_INFINITE_SSTHRESH;
    tp->snd_cwnd = 1;
    tp->prior_cwnd = 0;
    tp->high_seq = 0;
}

u32 tcp_reno_ssthresh(struct sock *sk)
{
    const struct tcp_sock *tp = tcp_sk(sk);
    u32 new_ssthresh = max(tp->snd_cwnd >> 1U, 2U);
    return new_ssthresh;
}

void tcp_reno_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
    struct tcp_sock *tp = tcp_sk(sk);
    printk(KERN_INFO "tp->snd_cwnd is %d\n", tp->snd_cwnd);

    if (tcp_dupack_count(tp) >= 3) {
        tcp_enter_recovery(sk, false);
        tp->prior_cwnd = tp->snd_cwnd;
        tp->snd_cwnd = tp->snd_ssthresh;
    }

    if (tcp_is_cwnd_limited(sk)) {
        if (tp->snd_cwnd <= tp->snd_ssthresh) {
            acked = tcp_slow_start(tp, acked);
            if (!acked)
                return;
        } else {
            tcp_cong_avoid_ai(tp, tp->snd_cwnd, acked);
        }
        tp->snd_cwnd = min(tp->snd_cwnd, tp->snd_cwnd_clamp);
    }
}

void tcp_reno_event_ack(struct sock *sk, u32 ack)
{
    struct tcp_sock *tp = tcp_sk(sk);

    if (tcp_in_recovery(tp)) {
        tp->snd_cwnd++;
        if (after(ack, tp->high_seq)) {
            tcp_end_recovery(sk);
            tp->snd_cwnd = tp->prior_cwnd;
        }
    }
}

u32 tcp_reno_undo_cwnd(struct sock *sk)
{
    u32 prior_cwnd = tcp_sk(sk)->prior_cwnd;
    return prior_cwnd;
}

static struct tcp_congestion_ops tcp_reno_custom = {
    .init           = tcp_reno_init,
    .ssthresh       = tcp_reno_ssthresh,
    .cong_avoid     = tcp_reno_cong_avoid,
    .undo_cwnd      = tcp_reno_undo_cwnd,
    .owner          = THIS_MODULE,
    .name           = "reno_custom",
};

static int __init tcp_reno_module_init(void)
{
    return tcp_register_congestion_control(&tcp_reno_custom);
}

static void __exit tcp_reno_module_exit(void)
{
    tcp_unregister_congestion_control(&tcp_reno_custom);
}

module_init(tcp_reno_module_init);
module_exit(tcp_reno_module_exit);

MODULE_AUTHOR("nethw");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Enhanced TCP Reno with Fast Retransmit, Fast Recovery, and Selective Repeat");
