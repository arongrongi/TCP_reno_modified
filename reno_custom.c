#include <linux/module.h>
#include <linux/kernel.h>
#include <net/tcp.h>

void tcp_reno_init(struct sock *sk)
{
    tcp_sk(sk)->snd_ssthresh = TCP_INFINITE_SSTHRESH;
    tcp_sk(sk)->snd_cwnd = 1;
    tcp_sk(sk)->prior_cwnd = 0;
    tcp_sk(sk)->high_seq = 0;
}

u32 tcp_reno_ssthresh(struct sock *sk)
{
    const struct tcp_sock *tp = tcp_sk(sk);
    return max(tp->snd_cwnd >> 1U, 2U);
}

void tcp_reno_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
    struct tcp_sock *tp = tcp_sk(sk);

    if (tcp_sk(sk)->snd_nxt - tcp_sk(sk)->snd_una >= 3) {
        printk(KERN_INFO "Fast Retransmit triggered\n");
        tcp_enter_loss(sk);
        tp->prior_cwnd = tp->snd_cwnd;
        tp->snd_cwnd = tp->snd_ssthresh;
        tcp_send_loss_probe(sk);
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

    if (tcp_in_cwnd_reduction(sk)) {
        tp->snd_cwnd++;
        if (after(ack, tp->high_seq)) {
            tp->snd_cwnd = tp->prior_cwnd;
        }
    }
}

u32 tcp_reno_undo_cwnd(struct sock *sk)
{
    return tcp_sk(sk)->prior_cwnd;
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
    if (tcp_register_congestion_control(&tcp_reno_custom))
        return -ENOBUFS;
    return 0;
}

static void __exit tcp_reno_module_exit(void)
{
    tcp_unregister_congestion_control(&tcp_reno_custom);
}

module_init(tcp_reno_module_init);
module_exit(tcp_reno_module_exit);

MODULE_AUTHOR("nethw");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Enhanced TCP Reno with Fast Retransmit, Fast Recovery, and SACK");
