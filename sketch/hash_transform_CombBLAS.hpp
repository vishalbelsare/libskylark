#ifndef SKYLARK_HASH_TRANSFORM_COMBBLAS_HPP
#define SKYLARK_HASH_TRANSFORM_COMBBLAS_HPP

#include <map>
#include "boost/serialization/map.hpp"

#include <CombBLAS.h>
#include "../utility/external/FullyDistMultiVec.hpp"
#include "../utility/exception.hpp"

#include "context.hpp"
#include "transforms.hpp"
#include "hash_transform_data.hpp"

namespace skylark { namespace sketch {

/* Specialization: FullyDistMultiVec for input, output */
template <typename IndexType,
          typename ValueType,
          template <typename> class IdxDistributionType,
          template <typename> class ValueDistribution>
struct hash_transform_t <FullyDistMultiVec<IndexType, ValueType>,
                         FullyDistMultiVec<IndexType, ValueType>,
                         IdxDistributionType,
                         ValueDistribution > :
        public hash_transform_data_t<IndexType,
                                     ValueType,
                                     IdxDistributionType,
                                     ValueDistribution> {
    typedef IndexType index_type;
    typedef ValueType value_type;
    typedef FullyDistMultiVec<IndexType, ValueType> matrix_type;
    typedef FullyDistMultiVec<IndexType, ValueType> output_matrix_type;
    typedef FullyDistVec<IndexType, ValueType> mpi_vector_t;
    typedef FullyDistMultiVec<IndexType, ValueType> mpi_multi_vector_t;
    typedef hash_transform_data_t<IndexType,
                                  ValueType,
                                  IdxDistributionType,
                                  ValueDistribution> base_data_t;

    /**
     * Regular constructor
     */
    hash_transform_t (int N, int S, skylark::sketch::context_t& context) :
        base_data_t (N, S, context) {}

    /**
     * Copy constructor
     */
    template <typename InputMatrixType,
              typename OutputMatrixType>
    hash_transform_t (hash_transform_t<InputMatrixType,
                                       OutputMatrixType,
                                       IdxDistributionType,
                                       ValueDistribution>& other) :
        base_data_t(other.get_data()) {}

    /**
     * Constructor from data
     */
    hash_transform_t (hash_transform_data_t<index_type,
                                            value_type,
                                            IdxDistributionType,
                                            ValueDistribution>& other_data) :
        base_data_t(other_data.get_data()) {}

    template <typename Dimension>
    void apply (const mpi_multi_vector_t &A,
        mpi_multi_vector_t &sketch_of_A,
        Dimension dimension) const {
        try {
            apply_impl (A, sketch_of_A, dimension);
        } catch(boost::mpi::exception e) {
            SKYLARK_THROW_EXCEPTION (
                utility::mpi_exception()
                    << utility::error_msg(e.what()) );
        } catch (std::string e) {
            SKYLARK_THROW_EXCEPTION (
                utility::combblas_exception()
                    << utility::error_msg(e) );
        } catch (std::logic_error e) {
            SKYLARK_THROW_EXCEPTION (
                utility::combblas_exception()
                    << utility::error_msg(e.what()) );
        }
    }


private:
    void apply_impl_single (const mpi_vector_t& a_,
        mpi_vector_t& sketch_of_a,
        columnwise_tag) const {
        std::vector<value_type> sketch_term(base_data_t::S,0);

        // We are essentially doing a 'const' access to a, but the neccessary,
        // 'const' option is missing from the interface.
        mpi_vector_t &a = const_cast<mpi_vector_t&>(a_);

        /** Accumulate the local sketch vector */
        /** FIXME: Lot's of random access --- not good for performance */
        DenseVectorLocalIterator<index_type, value_type> local_iter(a);
        while(local_iter.HasNext()) {
            index_type idx = local_iter.GetLocIndex();
            index_type global_idx = local_iter.LocalToGlobal(idx);
            index_type global_sketch_idx = base_data_t::row_idx[global_idx];
            sketch_term[global_sketch_idx] +=
                (local_iter.GetValue()*base_data_t::row_value[global_idx]);
            local_iter.Next();
        }

        /** Accumulate the global sketch vector */
        /** FIXME: Only need to scatter ... don't need everything everywhere */
        MPI_Allreduce(MPI_IN_PLACE,
            &(sketch_term[0]),
            base_data_t::S,
            MPIType<value_type>(),
            MPI_SUM,
            a.commGrid->GetWorld());

        /** Fill in .. SetElement() is dummy for non-local sets, so it's ok */
        for (index_type i=0; i<base_data_t::S; ++i) {
            sketch_of_a.SetElement(i,sketch_term[i]);
        }
    }


    void apply_impl (const mpi_multi_vector_t& A,
        mpi_multi_vector_t& sketch_of_A,
        columnwise_tag) const {
        const index_type num_rhs = A.size;
        if (sketch_of_A.size != num_rhs) { /** error */; return; }
        if (A.dim != base_data_t::N) { /** error */; return; }
        if (sketch_of_A.dim != base_data_t::S) { /** error */; return; }

        /** FIXME: Can sketch all the vectors in one shot */
        for (index_type i=0; i<num_rhs; ++i) {
            apply_impl_single (A[i], sketch_of_A[i], columnwise_tag());
        }
    }
};


/* Specialization: SpParMat for input, output */
template <typename IndexType,
          typename ValueType,
          template <typename> class IdxDistributionType,
          template <typename> class ValueDistribution>
struct hash_transform_t <
    SpParMat<IndexType, ValueType, SpDCCols<IndexType, ValueType> >,
    SpParMat<IndexType, ValueType, SpDCCols<IndexType, ValueType> >,
    IdxDistributionType,
    ValueDistribution > :
        public hash_transform_data_t<IndexType,
                                     ValueType,
                                     IdxDistributionType,
                                     ValueDistribution> {
    typedef IndexType index_type;
    typedef ValueType value_type;
    typedef SpDCCols< IndexType, value_type > col_t;
    typedef FullyDistVec< IndexType, ValueType > mpi_vector_t;
    typedef SpParMat< IndexType, value_type, col_t > matrix_type;
    typedef SpParMat< IndexType, value_type, col_t > output_matrix_type;
    typedef hash_transform_data_t<IndexType,
                                  ValueType,
                                  IdxDistributionType,
                                  ValueDistribution> base_data_t;


    /**
     * Regular constructor
     */
    hash_transform_t (int N, int S, skylark::sketch::context_t& context) :
        base_data_t(N, S, context) {}

    /**
     * Copy constructor
     */
    template <typename InputMatrixType,
              typename OutputMatrixType>
    hash_transform_t (hash_transform_t<InputMatrixType,
                                       OutputMatrixType,
                                       IdxDistributionType,
                                       ValueDistribution>& other) :
        base_data_t(other.get_data()) {}

    /**
     * Constructor from data
     */
    hash_transform_t (hash_transform_data_t<index_type,
                                            value_type,
                                            IdxDistributionType,
                                            ValueDistribution>& other_data) :
        base_data_t(other_data.get_data()) {}

    template <typename Dimension>
    void apply (const matrix_type &A,
        output_matrix_type &sketch_of_A,
        Dimension dimension) const {
        try {
            apply_impl (A, sketch_of_A, dimension);
        } catch(boost::mpi::exception e) {
            SKYLARK_THROW_EXCEPTION (
                utility::mpi_exception()
                    << utility::error_msg(e.what()) );
        } catch (std::string e) {
            SKYLARK_THROW_EXCEPTION (
                utility::combblas_exception()
                    << utility::error_msg(e) );
        } catch (std::logic_error e) {
            SKYLARK_THROW_EXCEPTION (
                utility::combblas_exception()
                    << utility::error_msg(e.what()) );
        }
    }


private:
    /**
     * Apply the sketching transform that is described in by the sketch_of_A.
     */
    template <typename Dimension>
    void apply_impl (const matrix_type &A_,
        output_matrix_type &sketch_of_A,
        Dimension dist) const {

        // We are essentially doing a 'const' access to A, but the neccessary,
        // 'const' option is missing from the interface
        matrix_type &A = const_cast<matrix_type&>(A_);

        const size_t rank = A.getcommgrid()->GetRank();

        // extract columns of matrix
        col_t &data = A.seq();

        const size_t ncols = sketch_of_A.getncol();
        const size_t nrows = sketch_of_A.getnrow();

        //FIXME: use comm_grid
        const size_t my_row_offset =
            static_cast<int>((static_cast<double>(A.getnrow()) /
                    A.getcommgrid()->GetGridRows())) *
            A.getcommgrid()->GetRankInProcCol(rank);

        const size_t my_col_offset =
            static_cast<int>((static_cast<double>(A.getncol()) /
                    A.getcommgrid()->GetGridCols())) *
            A.getcommgrid()->GetRankInProcRow(rank);

        // Pre-compute processor targets
        size_t nnz = 0;
        size_t comm_size = A.getcommgrid()->GetSize();
        // how many elements per processor
        std::vector< std::set<size_t> > proc_set(comm_size);
        for(typename col_t::SpColIter col = data.begcol();
            col != data.endcol(); col++) {
            for(typename col_t::SpColIter::NzIter nz = data.begnz(col);
                nz != data.endnz(col); nz++) {

                const index_type rowid = nz.rowid()  + my_row_offset;
                const index_type colid = col.colid() + my_col_offset;

                const size_t target_proc = compute_proc(A, rowid, colid, dist);
                const size_t pos         = getPos(rowid, colid, ncols, dist);

                if(proc_set[target_proc].count(pos) == 0) {
                    nnz++;
                    proc_set[target_proc].insert(pos);
                }
            }
        }

        // constructing arrays for one-sided access
        std::vector<size_t>     proc_size(comm_size, 0);
        std::vector<index_type> proc_start_idx(comm_size, 0);
        proc_size[0] = proc_set[0].size();
        for(size_t i = 1; i < proc_start_idx.size(); ++i) {
            proc_size[i]      = proc_set[i].size();
            proc_start_idx[i] = proc_start_idx[i-1] + proc_set[i-1].size();
        }

        std::vector<index_type> indicies(nnz, 0);
        std::vector<value_type> values(nnz, 0);

        // Apply sketch for all local values. Note that some of the resulting
        // values might end up on a different processor. The datastructure
        // fills values (sorted by processor id) in one continuous array.
        // Subsequently one-sided operations can be used to gather values for
        // each processor.
        for(typename col_t::SpColIter col = data.begcol();
            col != data.endcol(); col++) {
            for(typename col_t::SpColIter::NzIter nz = data.begnz(col);
                nz != data.endnz(col); nz++) {

                const index_type rowid = nz.rowid()  + my_row_offset;
                const index_type colid = col.colid() + my_col_offset;
                const size_t pos       = getPos(rowid, colid, ncols, dist);

                // get offset in array for current element
                const size_t proc   = compute_proc(A, rowid, colid, dist);
                const size_t ar_idx = proc_start_idx[proc] +
                    std::distance(proc_set[proc].begin(), proc_set[proc].find(pos));

                indicies[ar_idx]  = pos;
                values[ar_idx]   += nz.value() * getRowValue(rowid, colid, dist);
            }
        }

#ifdef COMBBLAS_REDIST
        const size_t loc_matrix_size = values.size();
        mpi_vector_t cols(loc_matrix_size);
        mpi_vector_t rows(loc_matrix_size);
        mpi_vector_t vals(loc_matrix_size);

        for(size_t i = 0; i < loc_matrix_size; ++i) {
            cols.SetElement(i, indicies[i] % ncols);
            rows.SetElement(i, indicies[i] / ncols);
            vals.SetElement(i, values[i]);
        }

        output_matrix_type tmp(
                sketch_of_A.getnrow(), sketch_of_A.getncol(), rows, cols, vals);
        sketch_of_A = tmp;
#else
        // Creating windows for all relevant arrays
        ///FIXME: MPI-3 stuff?
        MPI_Win proc_win, idx_win, val_win;
        MPI_Win_create(&proc_size[0], sizeof(size_t) * comm_size,
                       sizeof(size_t), MPI_INFO_NULL,
                       A.getcommgrid()->GetWorld(), &proc_win);

        MPI_Win_create(&indicies[0], sizeof(index_type) * indicies.size(),
                       sizeof(index_type), MPI_INFO_NULL,
                       A.getcommgrid()->GetWorld(), &idx_win);

        MPI_Win_create(&values[0], sizeof(value_type) * values.size(),
                       sizeof(value_type), MPI_INFO_NULL,
                       A.getcommgrid()->GetWorld(), &val_win);

        MPI_Win_fence(0, proc_win);
        MPI_Win_fence(0, idx_win);
        MPI_Win_fence(0, val_win);


        // accumulate values from other procs
        std::map<size_t, value_type> vals_map;
        for(size_t p = 0; p < comm_size; ++p) {

            size_t num_values = 0;
            MPI_Get(&num_values, 1, boost::mpi::get_mpi_datatype<size_t>(),
                    p, rank, 1, boost::mpi::get_mpi_datatype<size_t>(),
                    proc_win);
            MPI_Win_fence(0, proc_win);

            std::vector<index_type> add_idx(num_values);
            std::vector<value_type> add_val(num_values);
            MPI_Get(&(add_idx[0]), num_values,
                    boost::mpi::get_mpi_datatype<index_type>(), p, rank,
                    num_values, boost::mpi::get_mpi_datatype<index_type>(), idx_win);
            MPI_Win_fence(0, idx_win);

            MPI_Get(&(add_val[0]), num_values,
                    boost::mpi::get_mpi_datatype<value_type>(), p, rank,
                    num_values, boost::mpi::get_mpi_datatype<value_type>(), val_win);
            MPI_Win_fence(0, val_win);

            if(num_values == 0) continue;

            for(size_t i = 0; i < num_values; ++i) {
                if(vals_map.count(add_idx[i]) != 0)
                    vals_map[add_idx[i]] += add_val[i];
                else
                    vals_map.insert(std::make_pair(add_idx[i], add_val[i]));
            }
        }

        //FIXME: we need a method to set spSeq or a public SparseCommon (see
        //       below)!
        //       For now the only way is to create a temporary matrix and use
        //       assign operator (deep copy).

        //vector < tuple<index_type, index_type, value_type> >
            //data_val ( comm_size );

        //// and fill into sketch matrix (we know that all data is local now)
        //typename std::map<size_t, value_type>::const_iterator itr;
        //for(itr = vals_map.begin(); itr != vals_map.end(); itr++, idx++) {
            //index_type lrow = itr->first % ncols - my_row_offset;
            //index_type lcol = itr->first / ncols - my_col_offset;
            //data_val.push_back(make_tuple(lrow, lcol, itr->second));
        //}

        //SpTuples<index_type, value_type> tmp_tpl(
            //vals_map.size(), A.getlocalrows(), A.getlocalcols(), &(data_val[0]));

        //delete sketch_of_A.spSeq;
        //sketch_of_A.spSeq = new col_t(tmp_tpl, false);

        //XXX: or use SparseCommon (should not communicate anything anymore)
        //sketch_of_A.SparseCommon(data_val, vals_map.size(),
                //sketch_of_A.getnrow(), sketch_of_A.getncol());


        const size_t loc_matrix_size = vals_map.size();
        mpi_vector_t cols(loc_matrix_size);
        mpi_vector_t rows(loc_matrix_size);
        mpi_vector_t vals(loc_matrix_size);
        size_t idx = 0;

        typename std::map<size_t, value_type>::const_iterator itr;
        for(itr = vals_map.begin(); itr != vals_map.end(); itr++, idx++) {
            cols.SetElement(idx, itr->first % ncols);
            rows.SetElement(idx, itr->first / ncols);
            vals.SetElement(idx, itr->second);
        }

        // create temporary matrix (no further communication should be
        // required because all the data is local) and use assign operator
        // (deep copy)..
        output_matrix_type tmp(
                sketch_of_A.getnrow(), sketch_of_A.getncol(), rows, cols, vals);
        sketch_of_A = tmp;

        MPI_Win_fence(0, proc_win);
        MPI_Win_fence(0, idx_win);
        MPI_Win_fence(0, val_win);

        MPI_Win_free(&proc_win);
        MPI_Win_free(&idx_win);
        MPI_Win_free(&val_win);
#endif
    }


    inline index_type getPos(index_type rowid, index_type colid, size_t ncols,
        columnwise_tag) const {
        return colid + ncols * base_data_t::row_idx[rowid];
    }

    inline index_type getPos(index_type rowid, index_type colid, size_t ncols,
        rowwise_tag) const {
        return rowid * ncols + base_data_t::row_idx[colid];
    }

    inline value_type getRowValue(index_type rowid, index_type colid,
        columnwise_tag) const {
        return base_data_t::row_value[rowid];
    }

    inline value_type getRowValue(index_type rowid, index_type colid,
        rowwise_tag) const {
        return base_data_t::row_value[colid];
    }

    //FIXME: move to comm_grid
    inline size_t compute_proc(const matrix_type &A, const index_type row,
                               const index_type col, columnwise_tag) const {

        const index_type trow = base_data_t::row_idx[row];
        const size_t rows_per_proc = static_cast<size_t>(
            (static_cast<double>(A.getnrow()) / A.getcommgrid()->GetGridRows()));

        return A.getcommgrid()->GetRank(
            static_cast<size_t>(trow / rows_per_proc),
            A.getcommgrid()->GetRankInProcCol());
    }

    //FIXME: move to comm_grid
    inline size_t compute_proc(const matrix_type &A, const index_type row,
                               const index_type col, rowwise_tag) const {

        const index_type tcol = base_data_t::row_idx[col];
        const size_t cols_per_proc = static_cast<size_t>(
            (static_cast<double>(A.getncol()) / A.getcommgrid()->GetGridCols()));

        return A.getcommgrid()->GetRank(
            A.getcommgrid()->GetRankInProcRow(),
            static_cast<size_t>(tcol / cols_per_proc));
    }

};

} } /** namespace skylark::sketch */

#endif // SKYLARK_HASH_TRANSFORM_COMBBLAS_HPP
