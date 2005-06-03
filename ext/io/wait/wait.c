/**********************************************************************

  io/wait.c -

  $Author$
  $Date$
  created at: Tue Aug 28 09:08:06 JST 2001

  All the files in this distribution are covered under the Ruby's
  license (see the file COPYING).

**********************************************************************/

#include "ruby.h"
#include "rubyio.h"

#include <sys/types.h>
#include FIONREAD_HEADER

static VALUE io_ready_p _((VALUE io));
static VALUE io_wait _((int argc, VALUE *argv, VALUE io));
void Init_wait _((void));

EXTERN struct timeval rb_time_interval _((VALUE time));

/*
=begin
= IO wait methods.
=end
 */

/*
=begin
--- IO#ready?
    returns non-nil if input available without blocking, or nil.
=end
*/
static VALUE
io_ready_p(io)
    VALUE io;
{
    OpenFile *fptr;
    int n;

    GetOpenFile(io, fptr);
    rb_io_check_readable(fptr);
    if (rb_io_read_pending(fptr)) return Qtrue;
    if (ioctl(fptr->fd, FIONREAD, &n)) rb_sys_fail(0);
    if (n > 0) return INT2NUM(n);
    return Qnil;
}

struct wait_readable_arg {
    rb_fdset_t fds;
    struct timeval *timeout;
};

#ifdef HAVE_RB_FD_INIT
static VALUE
wait_readable(p)
    VALUE p;
{
    struct wait_readable_arg *arg = (struct wait_readable_arg *)p;
    rb_fdset_t *fds = &arg->fds;

    return (VALUE)rb_thread_select(rb_fd_max(fds), rb_fd_ptr(fds), NULL, NULL, arg->timeout);
}
#endif

/*
=begin
--- IO#wait([timeout])
    waits until input available or timed out and returns self, or nil
    when EOF reached.
=end
*/
static VALUE
io_wait(argc, argv, io)
    int argc;
    VALUE *argv;
    VALUE io;
{
    OpenFile *fptr;
    struct wait_readable_arg arg;
    int fd, n;
    VALUE timeout;
    struct timeval timerec;

    GetOpenFile(io, fptr);
    rb_io_check_readable(fptr);
    rb_scan_args(argc, argv, "01", &timeout);
    if (NIL_P(timeout)) {
	arg.timeout = 0;
    }
    else {
	timerec = rb_time_interval(timeout);
	arg.timeout = &timerec;
    }

    if (rb_io_read_pending(fptr)) return Qtrue;
    fd = fptr->fd;
    rb_fd_init(&arg.fds);
    rb_fd_set(fd, &arg.fds);
#ifdef HAVE_RB_FD_INIT
    n = (int)rb_ensure(wait_readable, (VALUE)&arg,
		       (VALUE (*)_((VALUE)))rb_fd_term, (VALUE)&arg.fds);
#else
    n = rb_thread_select(fd + 1, rb_fd_ptr(&rd), NULL, NULL, tp)
#endif
    if (n < 0)
	rb_sys_fail(0);
    rb_io_check_closed(fptr);
    if (ioctl(fptr->fd, FIONREAD, &n)) rb_sys_fail(0);
    if (n > 0) return io;
    return Qnil;
}

void
Init_wait()
{
    rb_define_method(rb_cIO, "ready?", io_ready_p, 0);
    rb_define_method(rb_cIO, "wait", io_wait, -1);
}