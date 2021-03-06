#ifndef SKYLARK_SPARSE_VC_STAR_MATRIX_HPP
#define SKYLARK_SPARSE_VC_STAR_MATRIX_HPP

#include <map>
#include <memory>
#include <vector>

#include <El.hpp>

#include "sparse_dist_matrix.hpp"

namespace skylark { namespace base {

/**
 *  This implements a very crude sparse VC / STAR matrix using a CSC sparse
 *  matrix container to hold the local sparse matrix.
 */
template<typename ValueType=double>
struct sparse_vc_star_matrix_t : public sparse_dist_matrix_t<ValueType> {

    typedef sparse_dist_matrix_t<ValueType> base_t;

    sparse_vc_star_matrix_t(const El::Grid& grid = El::Grid())
        : base_t(0, 0, grid) {

        _setup_grid();
    }

    sparse_vc_star_matrix_t(
            El::Int n_rows, El::Int n_cols, const El::Grid& grid)
        : base_t(n_rows, n_cols, grid) {

        _setup_grid();
    }

private:

    void _setup_grid() {

        base_t::_row_align = 0;
        base_t::_col_align = 0;

        base_t::_row_stride = 1;
        base_t::_col_stride = base_t::_grid.VCSize();

        base_t::_col_shift = base_t::_grid.VCRank();
        base_t::_row_shift = 0;

        base_t::_col_rank = El::mpi::Rank(base_t::_grid.VCComm());
        base_t::_row_rank = El::mpi::Rank(El::mpi::COMM_SELF);
    }
};

} }

#endif
