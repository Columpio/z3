/*++
Copyright (c) 2011 Microsoft Corporation

Module Name:

    datatype_rewriter.cpp

Abstract:

    Basic rewriting rules for Datatypes.

Author:

    Leonardo (leonardo) 2011-04-06

Notes:

--*/
#include "ast/rewriter/datatype_rewriter.h"
#include "ast/for_each_expr.h"
namespace {
namespace contains_partial_accessor_proc_ns {
struct found {};
struct contains_partial_accessor {
  ast_manager &m;
  datatype_util m_dt;
  contains_partial_accessor(ast_manager &a_m) : m(a_m), m_dt(m) {}
  void operator()(expr *n) const {}
  void operator()(app *n) {
      if (m_dt.is_accessor(n) && is_app(n->get_arg(0)) && m_dt.get_accessor_constructor(n->get_decl()) != to_app(n->get_arg(0))->get_decl())
      throw found();
  }
};
} // namespace contains_partial_accessor_ns
bool contains_partial_accessor_app(expr *c, ast_manager &m) {
  contains_partial_accessor_proc_ns::contains_partial_accessor t(m);
  try {
    for_each_expr(t, c);
    return false;
  } catch (const contains_partial_accessor_proc_ns::found &) {
    return true;
  }
}
namespace contains_uninterp_proc_ns {
    struct found{};
    struct contains_uninterp_proc {
      ast_manager &m;
      datatype_util m_dt;
      contains_uninterp_proc(ast_manager &a_m) : m(a_m), m_dt(m) {}
      void operator()(expr *n) const {}
      void operator()(app *n) {
        if (m.is_considered_uninterpreted(n->get_decl()))
            throw found();
      }
    };
}
bool contains_uninterp(expr *c, ast_manager &m) {
    contains_uninterp_proc_ns::contains_uninterp_proc t(m);
    try {
        for_each_expr(t, c);
        return false;
    } catch (const contains_uninterp_proc_ns::found &) {
        return true;
    }
}
}
br_status datatype_rewriter::mk_app_core(func_decl * f, unsigned num_args, expr * const * args, expr_ref & result) {
    SASSERT(f->get_family_id() == get_fid());
    switch(f->get_decl_kind()) {
    case OP_DT_CONSTRUCTOR: return BR_FAILED;
    case OP_DT_RECOGNISER:
        SASSERT(num_args == 1);
        result = m_util.mk_is(m_util.get_recognizer_constructor(f), args[0]);
        return BR_REWRITE1;
    case OP_DT_IS:
        //
        // simplify is_cons(cons(x,y)) -> true
        // simplify is_cons(nil) -> false
        //
        SASSERT(num_args == 1);
        // During inductive generalization spacer drops literals like
        // is_nil(tail(nil)) even though tail(nil) has been set to nil. the
        // contains_partial_accessor_app() method prevents this from happening.
        // now both is_nil(tail(nil)) and is_insert(tail(nil)) are true.
        // TODO: fix this in a better way. Maybe use the model to interpret all
        // partial accessor applications before it hits the rewriter.
        if (!is_app(args[0]) || !m_util.is_constructor(to_app(args[0]))) {
            if (!is_app(args[0]) || contains_uninterp(args[0], m()) || !is_ground(args[0]) || contains_partial_accessor_app(args[0], m()))
                return BR_FAILED;
            else
                result = m().mk_false();
        }
        if (to_app(args[0])->get_decl() == m_util.get_recognizer_constructor(f))
            result = m().mk_true();
        else
            result = m().mk_false();
        return BR_DONE;
    case OP_DT_ACCESSOR: {
        // 
        // simplify head(cons(x,y)) -> x
        // 
        SASSERT(num_args == 1);
        if (!is_app(args[0]) || !m_util.is_constructor(to_app(args[0])))
            return BR_FAILED;
        
        app * a = to_app(args[0]);
        func_decl * c_decl = a->get_decl();
        if (c_decl != m_util.get_accessor_constructor(f))
            return BR_FAILED;
        ptr_vector<func_decl> const & acc = *m_util.get_constructor_accessors(c_decl);
        SASSERT(acc.size() == a->get_num_args());
        unsigned num = acc.size();
        for (unsigned i = 0; i < num; ++i) {
            if (f == acc[i]) {
                // found it.
                result = a->get_arg(i);
                return BR_DONE;
            }
        }
        UNREACHABLE();
        break;
    }
    case OP_DT_UPDATE_FIELD: {
        SASSERT(num_args == 2);
        if (!is_app(args[0]) || !m_util.is_constructor(to_app(args[0])))
            return BR_FAILED;
        app * a = to_app(args[0]);
        func_decl * c_decl = a->get_decl();
        func_decl * acc = m_util.get_update_accessor(f);
        if (c_decl != m_util.get_accessor_constructor(acc)) {
            result = a;
            return BR_DONE;
        }
        ptr_vector<func_decl> const & accs = *m_util.get_constructor_accessors(c_decl);
        SASSERT(accs.size() == a->get_num_args());
        unsigned num = accs.size();
        ptr_buffer<expr> new_args;
        for (unsigned i = 0; i < num; ++i) {
            
            if (acc == accs[i]) {
                new_args.push_back(args[1]);
            }
            else {
                new_args.push_back(a->get_arg(i));
            }
        }
        result = m().mk_app(c_decl, num, new_args.c_ptr());
        return BR_DONE;        
    }
    default:
        UNREACHABLE();
    }
    
    return BR_FAILED;
}

br_status datatype_rewriter::mk_eq_core(expr * lhs, expr * rhs, expr_ref & result) {
    if (!is_app(lhs) || !is_app(rhs) || !m_util.is_constructor(to_app(lhs)) || !m_util.is_constructor(to_app(rhs)))
        return BR_FAILED;
    if (to_app(lhs)->get_decl() != to_app(rhs)->get_decl()) {
        result = m().mk_false();
        return BR_DONE;
    }

    // Remark: In datatype_simplifier_plugin, we used
    // m_basic_simplifier to create '=' and 'and' applications in the
    // following code. This trick not guarantee that the final expression
    // will be fully simplified. 
    //
    // Example:
    // The assertion  
    // (assert (= (cons a1 (cons a2 (cons a3 (cons (+ a4 1) (cons (+ a5 c5) (cons a6 nil))))))
    //         (cons b1 (cons b2 (cons b3 (cons b4 (cons b5 (cons b6 nil))))))))
    // 
    // After applying asserted_formulas::reduce(), the following formula was generated.
    //
    //   (= a1 b1)
    //   (= a2 b2)
    //   (= a3 b3)
    //   (= (+ a4 (* (- 1) b4)) (- 1))
    //   (= (+ c5 a5) b5)                    <<< NOT SIMPLIFIED WITH RESPECT TO ARITHMETIC
    //   (= (cons a6 nil) (cons b6 nil)))    <<< NOT SIMPLIFIED WITH RESPECT TO DATATYPE theory
    //
    // Note that asserted_formulas::reduce() applied the simplifier many times.
    // After the first simplification step we had:
    //  (= a1 b1)
    //  (= (cons a2 (cons a3 (cons (+ a4 1) (cons (+ a5 c5) (cons a6 nil))))))
    //     (cons b2 (cons b3 (cons b4 (cons b5 (cons b6 nil))))))

    ptr_buffer<expr> eqs;
    unsigned num = to_app(lhs)->get_num_args();
    SASSERT(num == to_app(rhs)->get_num_args());
    for (unsigned i = 0; i < num; ++i) {            
        eqs.push_back(m().mk_eq(to_app(lhs)->get_arg(i), to_app(rhs)->get_arg(i)));
    }
    result = m().mk_and(eqs.size(), eqs.c_ptr());
    return BR_REWRITE2;
}
