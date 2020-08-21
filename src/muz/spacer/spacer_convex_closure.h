#pragma once
/**++
Copyright (c) 2020 Arie Gurfinkel

Module Name:

    spacer_convex_closure.h

Abstract:

   Compute convex closure of polyhedra

Author:

    Hari Govind
    Arie Gurfinkel

Notes:

--*/

#include "ast/arith_decl_plugin.h"
#include "ast/ast.h"
#include "ast/ast_util.h"
#include "muz/spacer/spacer_arith_kernel.h"
#include "muz/spacer/spacer_matrix.h"
#include "muz/spacer/spacer_util.h"
#include "util/statistics.h"

namespace spacer {

class convex_closure {
    struct stats {
        unsigned m_num_reductions;
        unsigned m_max_dim;
        stopwatch watch;
        stats() { reset(); }
        void reset() {
            m_num_reductions = 0;
            m_max_dim = 0;
            watch.reset();
        }
    };
    stats m_st;

    ast_manager &m;
    arith_util m_arith;
    bv_util m_bv;

    // size of all bit vectors in m_dim_vars
    unsigned m_bv_sz;

    // true if \p m_dim_vars are arithmetic sort (i.e., Real or Int)
    bool m_is_arith;

    // size of \p m_dim_vars
    unsigned m_dim;

    // A vector of rational valued points
    spacer_matrix m_data;

    // Variables naming dimensions in `m_data`
    // \p m_dim_vars[i] is variable naming dimension \p i
    var_ref_vector m_dim_vars;

    // Kernel of \p m_data
    // Set at the end of computation
    spacer_arith_kernel m_kernel;

    // Free variables introduced by syntactic convex closure
    // These variables are always of sort Real
    var_ref_vector m_nw_vars;

    // Least common multiple of all coordinates in \p m_data
    rational m_lcm;

    // AG: Needs better comment
    // call m_kernel to reduce dimensions of m_data
    // return the rank of m_data
    unsigned reduce_dim();

    /// Construct all linear equations implied by points in \p m_data
    // This is defined by \p m_kernel * m_dim_vars = 0
    void generate_lin_deps(expr_ref_vector &res);

    /// Compute syntactic convex closure of \p m_data
    void syn_cls(expr_ref_vector &res_vec);

    // AG: better comment
    /// add (Col_j . m_nw_vars = m_dim_vars[j]) to res_vec
    void add_sum_cnstr(unsigned j, expr_ref_vector &res_vec);

    /// Compute one dimensional convex closure
    /// AG: what is \p var? 
    void do_one_dim_cls(expr_ref var, expr_ref_vector &res);

    /// Finds the largest numbers \p m and \p d such that \p m_data[i] mod m = d
    /// Returns true if successful
    bool compute_div_constraint(const vector<rational> &data, rational &m,
                                rational &d);

    /// Constructs a formula \p var ~ bnd, where  ~ = is_le ? <= : >=
    expr *mk_ineq(expr_ref var, rational bnd, bool is_le);

  public:
    convex_closure(ast_manager &manager, bool use_sage)
        : m(manager), m_arith(m), m_bv(m), m_bv_sz(0), m_is_arith(true),
          m_dim(0), m_data(0, 0), m_dim_vars(m), m_kernel(m_data),
          m_nw_vars(m) {

        if (use_sage) m_kernel.set_plugin(mk_sage_plugin());
    }

    void reset(unsigned n_cols);

    /// Turn support for fixed sized bit-vectors of size \p sz
    void set_bv(unsigned sz) {
        SASSERT(sz > 0);
        m_is_arith = false;
        m_bv_sz = sz;
    }

    /// \brief Name dimension \p i by variable \p v
    void set_dimension(unsigned i, var *v) {
        SASSERT(i < dims());
        SASSERT(m_dim_vars[i] == nullptr);
        m_dim_vars[i] = v;
    }

    /// \brief Return number of dimensions of each point
    unsigned dims() const { return m_dim; }

    /// \brief Return constants introduced by the syntactic convex closure
    const var_ref_vector &get_nw_vars() const { return m_nw_vars; }

    /// \brief Add a one-dimensional point to convex closure
    void push_back(rational x) {
        SASSERT(dims() == 1);
        vector<rational> row;
        row.push_back(x);
        m_data.add_row(row);
    }

    /// \brief Add a two-dimensional point to convex closure
    void push_back(rational x, rational y) {
        SASSERT(dims() == 2);
        vector<rational> row;
        row.push_back(x);
        row.push_back(y);
        m_data.add_row(row);
    }

    /// \brief Add an n-dimensional point to convex closure
    void push_back(vector<rational> &point) {
        SASSERT(point.size() == dims());
        m_data.add_row(point);
    };

    /// \brief Compute convex closure of the current set of points
    ///
    /// Returns true if successful and \p res is an exact convex closure
    /// Returns false if \p res is an over-approximation of the convex closure
    bool closure(expr_ref_vector &res);

    void collect_statistics(statistics &st) const;

    void reset_statistics() { m_st.reset(); }

    /// Set the least common multiple of \p m_data
    /// AG: this should be computed internally
    void set_lcm(rational l) { m_lcm = l; }
};
} // namespace spacer
