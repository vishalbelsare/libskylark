#ifndef SKYLARK_HASH_TRANSFORM_COMBBLAS_HPP
#define SKYLARK_HASH_TRANSFORM_COMBBLAS_HPP

#include <map>
#include <boost/serialization/map.hpp>

#include <CombBLAS.h>
#include "../utility/external/FullyDistMultiVec.hpp"
#include "../base/exception.hpp"
#include "../base/sparse_matrix.hpp"

#include "../base/context.hpp"
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
                                  ValueDistribution> data_type;

    /**
     * Regular constructor
     */
    hash_transform_t (int N, int S, base::context_t& context) :
        data_type (N, S, context) {}

    /**
     * Copy constructor
     */
    template <typename InputMatrixType,
              typename OutputMatrixType>
    hash_transform_t (hash_transform_t<InputMatrixType,
                                       OutputMatrixType,
                                       IdxDistributionType,
                                       ValueDistribution>& other) :
        data_type(other) {}

    /**
     * Constructor from data
     */
    hash_transform_t (hash_transform_data_t<index_type,
                                            value_type,
                                            IdxDistributionType,
                                            ValueDistribution>& other_data) :
        data_type(other_data) {}

    template <typename Dimension>
    void apply (const mpi_multi_vector_t &A,
        mpi_multi_vector_t &sketch_of_A,
        Dimension dimension) const {
        try {
            apply_impl (A, sketch_of_A, dimension);
        } catch(boost::mpi::exception e) {
            SKYLARK_THROW_EXCEPTION (
                base::mpi_exception()
                    << base::error_msg(e.what()) );
        } catch (std::string e) {
            SKYLARK_THROW_EXCEPTION (
                base::combblas_exception()
                    << base::error_msg(e) );
        } catch (std::logic_error e) {
            SKYLARK_THROW_EXCEPTION (
                base::combblas_exception()
                    << base::error_msg(e.what()) );
        }
    }


private:
    void apply_impl_single (const mpi_vector_t& a_,
                            mpi_vector_t& sketch_of_a,
                            columnwise_tag) const {
        std::vector<value_type> sketch_term(data_type::_S,0);

        // We are essentially doing a 'const' access to a, but the neccessary,
        // 'const' option is missing from the interface.
        mpi_vector_t &a = const_cast<mpi_vector_t&>(a_);

        /** Accumulate the local sketch vector */
        /** FIXME: Lot's of random access --- not good for performance */
        DenseVectorLocalIterator<index_type, value_type> local_iter(a);
        while(local_iter.HasNext()) {
            index_type idx = local_iter.GetLocIndex();
            index_type global_idx = local_iter.LocalToGlobal(idx);
            index_type global_sketch_idx = data_type::row_idx[global_idx];
            sketch_term[global_sketch_idx] +=
                (local_iter.GetValue()*data_type::row_value[global_idx]);
            local_iter.Next();
        }

        /** Accumulate the global sketch vector */
        /** FIXME: Only need to scatter ... don't need everything everywhere */
        MPI_Allreduce(MPI_IN_PLACE,
            &(sketch_term[0]),
            data_type::_S,
            MPIType<value_type>(),
            MPI_SUM,
            a.commGrid->GetWorld());

        /** Fill in .. SetElement() is dummy for non-local sets, so it's ok */
        for (index_type i=0; i<data_type::_S; ++i) {
            sketch_of_a.SetElement(i,sketch_term[i]);
        }
    }


    void apply_impl (const mpi_multi_vector_t& A,
        mpi_multi_vector_t& sketch_of_A,
        columnwise_tag) const {
        const index_type num_rhs = A.size;
        if (sketch_of_A.size != num_rhs) { /** error */; return; }
        if (A.dim != data_type::_N) { /** error */; return; }
        if (sketch_of_A.dim != data_type::_S) { /** error */; return; }

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
    typedef SpDCCols< index_type, value_type > col_t;
    typedef FullyDistVec< index_type, value_type> mpi_vector_t;
    typedef SpParMat< index_type, value_type, col_t > matrix_type;
    typedef SpParMat< index_type, value_type, col_t > output_matrix_type;
    typedef hash_transform_data_t<IndexType,
                                  ValueType,
                                  IdxDistributionType,
                                  ValueDistribution> data_type;


    /**
     * Regular constructor
     */
    hash_transform_t (int N, int S, base::context_t& context) :
        data_type(N, S, context) {

    }

    /**
     * Copy constructor
     */
    template <typename InputMatrixType,
              typename OutputMatrixType>
    hash_transform_t (hash_transform_t<InputMatrixType,
                                       OutputMatrixType,
                                       IdxDistributionType,
                                       ValueDistribution>& other) :
        data_type(other) {}

    /**
     * Constructor from data
     */
    hash_transform_t (hash_transform_data_t<index_type,
                                            value_type,
                                            IdxDistributionType,
                                            ValueDistribution>& other_data) :
        data_type(other_data) {}

    template <typename Dimension>
    void apply (const matrix_type &A,
        output_matrix_type &sketch_of_A,
        Dimension dimension) const {
        try {
            apply_impl (A, sketch_of_A, dimension);
        } catch(boost::mpi::exception e) {
            SKYLARK_THROW_EXCEPTION (
                base::mpi_exception()
                    << base::error_msg(e.what()) );
        } catch (std::string e) {
            SKYLARK_THROW_EXCEPTION (
                base::combblas_exception()
                    << base::error_msg(e) );
        } catch (std::logic_error e) {
            SKYLARK_THROW_EXCEPTION (
                base::combblas_exception()
                    << base::error_msg(e.what()) );
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

        // We are essentially doing a 'const' access to A, but the necessary,
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

        //FIXME: move to comm_grid
        const size_t grows = sketch_of_A.getcommgrid()->GetGridRows();
        const size_t rows_per_proc = static_cast<size_t>(
            sketch_of_A.getnrow() / grows);

        const size_t gcols = sketch_of_A.getcommgrid()->GetGridCols();
        const size_t cols_per_proc = static_cast<size_t>(
            sketch_of_A.getncol() / gcols);


        size_t comm_size = A.getcommgrid()->GetSize();
        std::vector< std::set<size_t> > proc_set(comm_size);

        // pre-compute processor targets of local sketch application
        for(typename col_t::SpColIter col = data.begcol();
            col != data.endcol(); col++) {
            for(typename col_t::SpColIter::NzIter nz = data.begnz(col);
                nz != data.endnz(col); nz++) {

                // compute global row and column id, and compress in one
                // target position index
                const index_type rowid = nz.rowid()  + my_row_offset;
                const index_type colid = col.colid() + my_col_offset;
                const size_t pos       = getPos(rowid, colid, ncols, dist);

                // compute target processor for this target index
                const size_t target = sketch_of_A.getcommgrid()->GetRank(
                    std::min(static_cast<size_t>((pos / ncols) / rows_per_proc), grows - 1),
                    std::min(static_cast<size_t>((pos % ncols) / cols_per_proc), gcols - 1));

                if(proc_set[target].count(pos) == 0) {
                    assert(target < comm_size);
                    proc_set[target].insert(pos);
                }
            }
        }

        // constructing arrays for one-sided access
        std::vector<size_t>     proc_size(comm_size, 0);
        std::vector<index_type> proc_start_idx(comm_size, 0);
        proc_size[0] = proc_set[0].size();
        for(size_t i = 1; i < proc_start_idx.size(); ++i) {
            proc_size[i]      = proc_set[i].size();
            proc_start_idx[i] = proc_start_idx[i-1] + proc_size[i-1];
        }

        size_t nnz = proc_start_idx[comm_size-1] + proc_size[comm_size-1];
        std::vector<index_type> indicies(nnz, 0);
        std::vector<value_type> values(nnz, 0);

        // Apply sketch for all local values. Note that some of the resulting
        // values might end up on a different processor. The data structure
        // fills values (sorted by processor id) in one continuous array.
        // Subsequently, one-sided operations can be used to access values for
        // each processor.
        for(typename col_t::SpColIter col = data.begcol();
            col != data.endcol(); col++) {
            for(typename col_t::SpColIter::NzIter nz = data.begnz(col);
                nz != data.endnz(col); nz++) {

                // compute global row and column id, and compress in one
                // target position index
                const index_type rowid = nz.rowid()  + my_row_offset;
                const index_type colid = col.colid() + my_col_offset;
                const size_t pos       = getPos(rowid, colid, ncols, dist);

                // compute target processor for this target index
                const size_t proc = sketch_of_A.getcommgrid()->GetRank(
                    std::min(static_cast<size_t>((pos / ncols) / rows_per_proc), grows - 1),
                    std::min(static_cast<size_t>((pos % ncols) / cols_per_proc), gcols - 1));

                // get offset in array for current element
                const size_t ar_idx = proc_start_idx[proc] +
                    std::distance(proc_set[proc].begin(), proc_set[proc].find(pos));

                indicies[ar_idx] = pos;
                values[ar_idx]  += nz.value() * getRowValue(rowid, colid, dist);
            }
        }

        // Creating windows for all relevant arrays
        ///FIXME: MPI-3 stuff?
        MPI_Win proc_win, start_offset_win, idx_win, val_win;
        MPI_Win_create(&proc_size[0], sizeof(size_t) * comm_size,
                       sizeof(size_t), MPI_INFO_NULL,
                       A.getcommgrid()->GetWorld(), &proc_win);

        MPI_Win_create(&proc_start_idx[0], sizeof(size_t) * comm_size,
                       sizeof(size_t), MPI_INFO_NULL,
                       A.getcommgrid()->GetWorld(), &start_offset_win);

        MPI_Win_create(&indicies[0], sizeof(index_type) * indicies.size(),
                       sizeof(index_type), MPI_INFO_NULL,
                       A.getcommgrid()->GetWorld(), &idx_win);

        MPI_Win_create(&values[0], sizeof(value_type) * values.size(),
                       sizeof(value_type), MPI_INFO_NULL,
                       A.getcommgrid()->GetWorld(), &val_win);

        MPI_Win_fence(0, proc_win);
        MPI_Win_fence(0, start_offset_win);
        MPI_Win_fence(0, idx_win);
        MPI_Win_fence(0, val_win);


        // accumulate values from other procs
        std::map<size_t, value_type> vals_map;
        for(size_t p = 0; p < comm_size; ++p) {

            // since all procs need to call the fence we gather all the
            // necessary values
            size_t num_values = 0;
            MPI_Get(&num_values, 1, boost::mpi::get_mpi_datatype<size_t>(),
                    p, rank, 1, boost::mpi::get_mpi_datatype<size_t>(),
                    proc_win);
            MPI_Win_fence(0, proc_win);

            size_t offset = 0;
            MPI_Get(&offset, 1, boost::mpi::get_mpi_datatype<size_t>(),
                    p, rank, 1, boost::mpi::get_mpi_datatype<size_t>(),
                    start_offset_win);
            MPI_Win_fence(0, start_offset_win);

            // since all procs need to call the fence we fill indices and
            // values even if num_values can be 0 (= don't get data).
            std::vector<index_type> add_idx(num_values);
            std::vector<value_type> add_val(num_values);
            MPI_Get(&(add_idx[0]), num_values,
                    boost::mpi::get_mpi_datatype<index_type>(), p, offset,
                    num_values, boost::mpi::get_mpi_datatype<index_type>(),
                    idx_win);
            MPI_Win_fence(0, idx_win);

            MPI_Get(&(add_val[0]), num_values,
                    boost::mpi::get_mpi_datatype<value_type>(), p, offset,
                    num_values, boost::mpi::get_mpi_datatype<value_type>(),
                    val_win);
            MPI_Win_fence(0, val_win);

            // finally, add data to local buffer (if we have any).
            for(size_t i = 0; i < num_values; ++i) {
                if(vals_map.count(add_idx[i]) != 0)
                    vals_map[add_idx[i]] += add_val[i];
                else
                    vals_map.insert(std::make_pair(add_idx[i], add_val[i]));
            }
        }

        create_local_sp_mat(vals_map, sketch_of_A);

        MPI_Win_fence(0, proc_win);
        MPI_Win_fence(0, start_offset_win);
        MPI_Win_fence(0, idx_win);
        MPI_Win_fence(0, val_win);

        MPI_Win_free(&proc_win);
        MPI_Win_free(&start_offset_win);
        MPI_Win_free(&idx_win);
        MPI_Win_free(&val_win);
    }


    //FIXME: move to util
    /** Create a sparse CombBLAS matrix given a mapping of local indices to
     *  values. The 1D index is defined as:
     *
     *      index = row_index * num_cols + col_index
     *
     *  where num_cols denotes the number of columns in the output matrix.
     *
     *  FIXME: is there a direct way to set the "data buffer" for the output
     *         matrix? Currently this method creates a temporary matrix and
     *         then assigns to the output matrix (deep copy).
     */
    void create_local_sp_mat(std::map<size_t, value_type> &vals_map,
                             output_matrix_type &matrix) const {

        const size_t ncols = matrix.getncol();
        const size_t rank  = matrix.getcommgrid()->GetRank();

        // creating a local structure to hold sparse data triplets
        std::vector< tuple< index_type, index_type, value_type > > data;

        // in order to convert global row/col index to local we need to know
        // the row/col offsets for each processor.
        const size_t row_offset =
            static_cast<size_t>((static_cast<double>(matrix.getnrow()) /
                    matrix.getcommgrid()->GetGridRows())) *
                    matrix.getcommgrid()->GetRankInProcCol(rank);
        const size_t col_offset =
            static_cast<size_t>((static_cast<double>(ncols) /
                    matrix.getcommgrid()->GetGridCols())) *
                    matrix.getcommgrid()->GetRankInProcRow(rank);

        // fill into sketch matrix
        typename std::map<size_t, value_type>::const_iterator itr;
        for(itr = vals_map.begin(); itr != vals_map.end(); itr++) {
            index_type lrow = itr->first / ncols - row_offset;
            index_type lcol = itr->first % ncols - col_offset;
            value_type val  = itr->second;
            data.push_back(make_tuple(lrow, lcol, val));
        }

        // this pointer will be freed in the destructor of col_t (see below).
        //FIXME: verify with Valgrind
        SpTuples<index_type, value_type> *tmp_tpl =
            new SpTuples<index_type, value_type> (
                data.size(), matrix.getlocalrows(), matrix.getlocalcols(),
                &data[0]);

        // create temporary matrix (no further communication should be
        // required because all the data are local) and assign (deep copy).
        col_t *sp_data = new col_t(*tmp_tpl, false);
        matrix         = output_matrix_type(sp_data, matrix.getcommgrid());
    }


    inline index_type getPos(index_type rowid, index_type colid, size_t ncols,
        columnwise_tag) const {
        return colid + ncols * data_type::row_idx[rowid];
    }

    inline index_type getPos(index_type rowid, index_type colid, size_t ncols,
        rowwise_tag) const {
        return rowid * ncols + data_type::row_idx[colid];
    }

    inline value_type getRowValue(index_type rowid, index_type colid,
        columnwise_tag) const {
        return data_type::row_value[rowid];
    }

    inline value_type getRowValue(index_type rowid, index_type colid,
        rowwise_tag) const {
        return data_type::row_value[colid];
    }
};


/* Specialization: SpParMat for input, Local SpMat output */
template <typename IndexType,
          typename ValueType,
          template <typename> class IdxDistributionType,
          template <typename> class ValueDistribution>
struct hash_transform_t <
    SpParMat<IndexType, ValueType, SpDCCols<IndexType, ValueType> >,
    base::sparse_matrix_t<ValueType>,
    IdxDistributionType,
    ValueDistribution > :
        public hash_transform_data_t<IndexType,
                                     ValueType,
                                     IdxDistributionType,
                                     ValueDistribution> {
    typedef IndexType index_type;
    typedef ValueType value_type;
    typedef SpDCCols< index_type, value_type > col_t;
    typedef FullyDistVec< index_type, value_type> mpi_vector_t;
    typedef SpParMat< index_type, value_type, col_t > matrix_type;
    typedef base::sparse_matrix_t< value_type > output_matrix_type;
    typedef hash_transform_data_t<IndexType,
                                  ValueType,
                                  IdxDistributionType,
                                  ValueDistribution> data_type;


    /**
     * Regular constructor
     */
    hash_transform_t (int N, int S, base::context_t& context) :
        data_type(N, S, context) {

    }

    /**
     * Copy constructor
     */
    template <typename InputMatrixType,
              typename OutputMatrixType>
    hash_transform_t (hash_transform_t<InputMatrixType,
                                       OutputMatrixType,
                                       IdxDistributionType,
                                       ValueDistribution>& other) :
        data_type(other) {}

    /**
     * Constructor from data
     */
    hash_transform_t (hash_transform_data_t<index_type,
                                            value_type,
                                            IdxDistributionType,
                                            ValueDistribution>& other_data) :
        data_type(other_data) {}

    template <typename Dimension>
    void apply (const matrix_type &A,
        output_matrix_type &sketch_of_A,
        Dimension dimension) const {
        try {
            apply_impl (A, sketch_of_A, dimension);
        } catch(boost::mpi::exception e) {
            SKYLARK_THROW_EXCEPTION (
                base::mpi_exception()
                    << base::error_msg(e.what()) );
        } catch (std::string e) {
            SKYLARK_THROW_EXCEPTION (
                base::combblas_exception()
                    << base::error_msg(e) );
        } catch (std::logic_error e) {
            SKYLARK_THROW_EXCEPTION (
                base::combblas_exception()
                    << base::error_msg(e.what()) );
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

        // We are essentially doing a 'const' access to A, but the necessary,
        // 'const' option is missing from the interface
        matrix_type &A = const_cast<matrix_type&>(A_);

        const size_t rank = A.getcommgrid()->GetRank();

        // extract columns of matrix
        col_t &data = A.seq();

        //FIXME: use comm_grid
        const size_t my_row_offset =
            static_cast<int>((static_cast<double>(A.getnrow()) /
                    A.getcommgrid()->GetGridRows())) *
                    A.getcommgrid()->GetRankInProcCol(rank);

        const size_t my_col_offset =
            static_cast<int>((static_cast<double>(A.getncol()) /
                    A.getcommgrid()->GetGridCols())) *
                    A.getcommgrid()->GetRankInProcRow(rank);


        int n_res_cols = A.getncol();
        int n_res_rows = A.getnrow();
        get_res_size(n_res_rows, n_res_cols, dist);

        // Apply sketch for all local values. Subsequently, all values are
        // gathered on processor 0 and the local matrix is populated.
        // XXX: For now sort on each processor to ease the creation of the
        // gathered local matrix. Most likely possible can be avoided by
        // traversing the matrix in the correct order.
        typedef std::map<index_type, value_type> col_values_t;
        col_values_t col_values;
        for(typename col_t::SpColIter col = data.begcol();
            col != data.endcol(); col++) {
            for(typename col_t::SpColIter::NzIter nz = data.begnz(col);
                nz != data.endnz(col); nz++) {

                index_type rowid = nz.rowid()  + my_row_offset;
                index_type colid = col.colid() + my_col_offset;

                const value_type value =
                    nz.value() * getValue(rowid, colid, dist);
                finalPos(rowid, colid, dist);
                col_values[colid * n_res_rows + rowid] += value;
            }
        }

        boost::mpi::communicator world(A.getcommgrid()->GetWorld(),
                                       boost::mpi::comm_duplicate);

        std::vector< std::map<index_type, value_type > >
            result;
        boost::mpi::gather(world, col_values, result, 0);

        // unpack into temp structure
        //FIXME:
        int size_estimate = A.getnnz();
        if(rank == 0) {
            create_local_sp_mat(result, n_res_rows, n_res_cols, size_estimate,
                                sketch_of_A);
        }
    }

    inline void finalPos(index_type &rowid, index_type &colid, columnwise_tag) const {
        rowid = data_type::row_idx[rowid];
    }

    inline void finalPos(index_type &rowid, index_type &colid, rowwise_tag) const {
        colid = data_type::row_idx[colid];
    }

    inline value_type getValue(index_type rowid, index_type colid,
                               columnwise_tag) const {
        return data_type::row_value[rowid];
    }

    inline value_type getValue(index_type rowid, index_type colid,
                               rowwise_tag) const {
        return data_type::row_value[colid];
    }

    inline void get_res_size(int &rows, int &cols, columnwise_tag) const {
        rows = data_type::_S;
    }

    inline void get_res_size(int &rows, int &cols, rowwise_tag) const {
        cols = data_type::_S;
    }


    void create_local_sp_mat(std::vector< std::map<index_type, value_type > > &result,
                             int n_res_rows, int n_res_cols, int size_estimate,
                             output_matrix_type &sketch_of_A) const {

        int nnz = 0;
        int *indptr_new = new int[n_res_cols + 1];
        std::vector<int> final_rows(size_estimate);
        std::vector<value_type> final_vals(size_estimate);

        indptr_new[0] = 0;

        typedef typename std::map<index_type, value_type>::iterator itr_t;
        std::vector<itr_t> proc_iters(result.size());
        for(size_t i = 0; i < result.size(); ++i)
            proc_iters[i] = result[i].begin();

        std::vector<index_type> idx_map(n_res_rows, -1);

        for(int col = 0; col < n_res_cols; ++col) {

            // collect all values for column 'col' of all procs
            for(size_t i = 0; i < result.size(); ++i) {

                itr_t cur_itr = proc_iters[i];

                for(; cur_itr != result[i].end() &&
                        static_cast<int>(cur_itr->first / n_res_rows) == col;
                        cur_itr++) {

                    int row    = cur_itr->first % n_res_rows;
                    double val = cur_itr->second;

                    if(idx_map[row] == -1) {
                        idx_map[row] = nnz;
                        final_rows[nnz] = row;
                        final_vals[nnz] = val;
                        nnz++;
                    } else {
                        final_vals[idx_map[row]] += val;
                    }
                }

                proc_iters[i] = cur_itr;
            }

            indptr_new[col + 1] = nnz;

            // reset idx_map
            for(int i = indptr_new[col]; i < nnz; ++i)
                idx_map[final_rows[i]] = -1;
        }

        int *indices_new = new int[nnz];
        std::copy(final_rows.begin(), final_rows.begin() + nnz, indices_new);

        double *values_new = new double[nnz];
        std::copy(final_vals.begin(), final_vals.begin() + nnz, values_new);

        sketch_of_A.attach(indptr_new, indices_new, values_new,
                nnz, n_res_rows, n_res_cols, true);
    }
};

} } /** namespace skylark::sketch */

#endif // SKYLARK_HASH_TRANSFORM_COMBBLAS_HPP
