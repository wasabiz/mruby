#include "mruby.h"
#include "mruby/proc.h"

#ifndef FIBER_STACK_INIT_SIZE
#  define FIBER_STACK_INIT_SIZE 64
#endif
#ifndef FIBER_CI_INIT_SIZE
#  define FIBER_CI_INIT_SIZE 8
#endif

/* mark return from context modifying method */
#define MARK_CONTEXT_MODIFY(c) (c)->ci->target_class = NULL

static mrb_value
mrb_kernel_reset(mrb_state* mrb, mrb_value self)
{
  static const struct mrb_context mrb_context_zero = { 0 };
  struct mrb_context *c;
  struct RProc *p;
  mrb_callinfo *ci;
  mrb_value blk;

  mrb_get_args(mrb, "&", &blk);

  if (mrb_nil_p(blk)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "tried to reset without a block");
  }
  p = mrb_proc_ptr(blk);
  if (MRB_PROC_CFUNC_P(p)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "tried to reset from C defined method");
  }

  c = (struct mrb_context*)mrb_malloc(mrb, sizeof(struct mrb_context));
  *c = mrb_context_zero;

  /* initialize VM stack */
  c->stbase = (mrb_value *)mrb_calloc(mrb, FIBER_STACK_INIT_SIZE, sizeof(mrb_value));
  c->stend = c->stbase + FIBER_STACK_INIT_SIZE;
  c->stack = c->stbase;

  /* copy receiver from a block */
  c->stack[0] = mrb->c->stack[0];

  /* initialize callinfo stack */
  c->cibase = (mrb_callinfo *)mrb_calloc(mrb, FIBER_CI_INIT_SIZE, sizeof(mrb_callinfo));
  c->ciend = c->cibase + FIBER_CI_INIT_SIZE;
  c->ci = c->cibase;

  /* adjust return callinfo */
  ci = c->ci;
  ci->target_class = p->target_class;
  ci->proc = p;
  ci->pc = p->body.irep->iseq;
  ci->nregs = p->body.irep->nregs;
  ci[1] = ci[0];
  c->ci++;                      /* push dummy callinfo */

  c->fib = NULL;

  /* delegate process */
  mrb->c->status = MRB_FIBER_RESUMED;
  c->cibase->argc = 0;		/* block with no args */
  c->prev = mrb->c;
  c->status = MRB_FIBER_RUNNING;
  mrb->c = c;

  MARK_CONTEXT_MODIFY(c);
  return c->ci->proc->env->stack[0];
}

void
mrb_mruby_partcont_gem_init(mrb_state* mrb)
{
  mrb_define_class_method(mrb, mrb->kernel_module, "reset", mrb_kernel_reset, MRB_ARGS_BLOCK());
  mrb_define_method(mrb, mrb->kernel_module, "reset", mrb_kernel_reset, MRB_ARGS_BLOCK());
}

void
mrb_mruby_partcont_gem_final(mrb_state* mrb)
{
}
