/**
 *  This test ensures that the sketch application (for CombBLAS matrices) is
 *  done correctly (on-the-fly matrix multiplication in the code is compared
 *  to true matrix multiplication).
 *  This test builds on the following assumptions:
 *
 *      - CombBLAS PSpGEMM returns the correct result, and
 *      - the random numbers in row_idx and row_value (see
 *        hash_transform_data_t) are drawn from the promised distributions.
 */


#include <vector>

#include <boost/mpi.hpp>
#include <boost/test/minimal.hpp>

#include "../../utility/distributions.hpp"
#include "../../base/sparse_matrix.hpp"
#include "../../base/context.hpp"
#include "../../sketch/hash_transform.hpp"


template < typename InputMatrixType,
           typename OutputMatrixType = InputMatrixType >
struct Dummy_t : public skylark::sketch::hash_transform_t<
    InputMatrixType, OutputMatrixType,
    boost::random::uniform_int_distribution,
    skylark::utility::rademacher_distribution_t > {

    typedef skylark::sketch::hash_transform_t<
        InputMatrixType, OutputMatrixType,
        boost::random::uniform_int_distribution,
        skylark::utility::rademacher_distribution_t >
            hash_t;

    Dummy_t(int N, int S, skylark::base::context_t& context)
        : skylark::sketch::hash_transform_t<InputMatrixType, OutputMatrixType,
          boost::random::uniform_int_distribution,
          skylark::utility::rademacher_distribution_t>(N, S, context)
    {}

    std::vector<size_t> getRowIdx() { return hash_t::row_idx; }
    std::vector<double> getRowValues() { return hash_t::row_value; }
};

int test_main(int argc, char *argv[]) {

    //////////////////////////////////////////////////////////////////////////
    //[> Parameters <]

    //FIXME: use random sizes?
    const size_t n   = 10;
    const size_t m   = 5;
    const size_t n_s = 6;
    const size_t m_s = 3;

    typedef skylark::base::sparse_matrix_t<double> Matrix_t;

    //////////////////////////////////////////////////////////////////////////
    //[> Setup test <]

    namespace mpi = boost::mpi;
    mpi::environment env(argc, argv);
    mpi::communicator world;
    const size_t rank = world.rank();

    skylark::base::context_t context (0);

    double count = 1.0;

    const int matrix_full = n * m;
    std::vector<int> colsf(m + 1);
    std::vector<int> rowsf(matrix_full);
    std::vector<double> valsf(matrix_full);

    for(size_t i = 0; i < m + 1; ++i)
        colsf[i] = i * n;

    for(size_t i = 0; i < matrix_full; ++i) {
        rowsf[i] = i % n;
        valsf[i] = count;
        count++;
    }

    Matrix_t A;
    A.attach(&colsf[0], &rowsf[0], &valsf[0], matrix_full, n, m, false);

    count = 1;
    const int* indptr = A.indptr();
    const int* indices = A.indices();
    const double* values = A.locked_values();

    for(int col = 0; col < A.width(); col++) {
        for(int idx = indptr[col]; idx < indptr[col + 1]; idx++) {
            BOOST_REQUIRE( values[idx] == count );
            count++;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    //[> Test COO to CSC <]
    typename Matrix_t::coords_t coords_mat;
    count = 1;
    for(int col = 0; col < A.width(); col++) {
        for(int row = 0; row < A.height(); row++) {
            typename Matrix_t::coord_tuple_t new_entry(row, col, count);
            coords_mat.push_back(new_entry);
            count++;
        }
    }

    Matrix_t A_coord;
    A_coord.set(coords_mat);

    count = 1;
    indptr = A_coord.indptr();
    indices = A_coord.indices();
    values = A_coord.locked_values();

    for(int col = 0; col < A_coord.width(); col++) {
        for(int idx = indptr[col]; idx < indptr[col + 1]; idx++) {
            BOOST_REQUIRE( values[idx] == count );
            count++;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    //[> Column wise application <]

    //[> 1. Create the sketching matrix <]
    Dummy_t<Matrix_t, Matrix_t> Sparse(n, n_s, context);
    std::vector<size_t> row_idx = Sparse.getRowIdx();
    std::vector<double> row_val = Sparse.getRowValues();

    // PI generated by random number gen
    int sketch_size = row_val.size();
    typename Matrix_t::coords_t coords;
    for(int i = 0; i < sketch_size; ++i) {
        typename Matrix_t::coord_tuple_t new_entry(row_idx[i], i, row_val[i]);
        coords.push_back(new_entry);
    }

    Matrix_t pi_sketch;
    pi_sketch.set(coords);

    //[> 2. Create sketched matrix <]
    Matrix_t sketch_A;

    //[> 3. Apply the transform <]
    Sparse.apply(A, sketch_A, skylark::sketch::columnwise_tag());

    BOOST_CHECK(sketch_A.height() == n_s);
    BOOST_CHECK(sketch_A.width()  == m);

    //[> 4. Build structure to compare: PI * A ?= sketch_A <]
    typename Matrix_t::coords_t coords_new;
    indptr = pi_sketch.indptr();
    indices = pi_sketch.indices();
    values = pi_sketch.locked_values();

    // multiply with vector where an entry has the value:
    //   col_idx * n + row_idx + 1.
    // See creation of A.
    for(int col = 0; col < pi_sketch.width(); col++) {
        for(int idx = indptr[col]; idx < indptr[col + 1]; idx++) {
            for(int ccol = 0; ccol < m; ++ccol) {
                typename Matrix_t::coord_tuple_t new_entry(indices[idx], ccol,
                    values[idx] * (ccol * n + col + 1));
                coords_new.push_back(new_entry);
            }
        }
    }

    Matrix_t expected_A;
    expected_A.set(coords_new, n_s, m);

    if (!static_cast<bool>(expected_A == sketch_A))
        BOOST_FAIL("Result of colwise application not as expected");


    //////////////////////////////////////////////////////////////////////////
    //[> Row wise application <]

    //[> 1. Create the sketching matrix <]
    Dummy_t<Matrix_t, Matrix_t> Sparse_row(m, m_s, context);
    row_idx = Sparse_row.getRowIdx();
    row_val = Sparse_row.getRowValues();

    // PI generated by random number gen
    sketch_size = row_val.size();
    coords.clear();
    for(int i = 0; i < sketch_size; ++i) {
        typename Matrix_t::coord_tuple_t new_entry(i, row_idx[i], row_val[i]);
        coords.push_back(new_entry);
    }

    Matrix_t pi_sketch_row;
    pi_sketch_row.set(coords);

    //[> 2. Create sketched matrix <]
    Matrix_t sketch_A_row;

    //[> 3. Apply the transform <]
    Sparse_row.apply(A, sketch_A_row, skylark::sketch::rowwise_tag());

    BOOST_CHECK(sketch_A_row.height() == n);
    BOOST_CHECK(sketch_A_row.width()  == m_s);

    //[> 4. Build structure to compare: A * PI ?= sketch_A <]
    coords_new.clear();
    indptr = pi_sketch_row.indptr();
    indices = pi_sketch_row.indices();
    values = pi_sketch_row.locked_values();

    // multiply with vector where an entry has the value:
    //   col_idx * n + row_idx + 1.
    // See creation of A.
    for(int col = 0; col < pi_sketch_row.width(); col++) {
        for(int idx = indptr[col]; idx < indptr[col + 1]; idx++) {
            for(int row = 0; row < n; ++row) {
                typename Matrix_t::coord_tuple_t new_entry(row, col,
                    values[idx] * (indices[idx] * n + row + 1));
                coords_new.push_back(new_entry);
            }
        }
    }

    Matrix_t expected_A_row;
    expected_A_row.set(coords_new, n, m_s);

    if (!static_cast<bool>(expected_A_row == sketch_A_row))
        BOOST_FAIL("Result of rowwise application not as expected");

    return 0;
}