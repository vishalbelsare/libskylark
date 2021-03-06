#XXX: for a clean exit we need to import KDT first because it initializes MPI
#     in any case
import kdt

import numpy
import scipy
import unittest

from mpi4py import MPI
from skylark import io

import elem

class IO_test(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        super(IO_test, self).__init__(*args, **kwargs)
        self.ele_A = elem.DistMatrix_d()
        elem.Uniform(self.ele_A, 10, 30)

        self.rank = MPI.COMM_WORLD.Get_rank()
        self.size = MPI.COMM_WORLD.Get_size()

        if self.rank == 0:
            self.np_A = numpy.random.random((20, 65))
            self.sp_A = scipy.sparse.rand(20, 65, density=0.2, format='csr')

    def compareMatrixNorm(self, A, B):
        # Gather at root
        A_CIRC_CIRC = elem.DistMatrix_d_CIRC_CIRC()
        elem.Copy(A, A_CIRC_CIRC)

        # Compare Frobenius norm
        #FIXME: some tests fail if we use higher accuracy -- why?
        if self.rank == 0:
            self.assertAlmostEqual(
                numpy.linalg.norm(A_CIRC_CIRC.Matrix[:], ord='fro'),
                numpy.linalg.norm(B, ord='fro'), 5)

    def test_mm(self):
        matrix_fpath = 'test_matrix.mtx'

        store = io.mtx(matrix_fpath)
        # root writes its scipy-sparse matrix...
        if self.rank == 0:
            store.write(self.sp_A)
        MPI.COMM_WORLD.barrier()
        # ... all processes read back what root has written
        B = store.read('combblas-sparse')
        C = store.read('scipy-sparse')
        D = store.read('numpy-dense')

        # convert CombBLAS matrix to (coo) sparse matrix
        col, row, data = B.toVec()
        sp_cb = scipy.sparse.coo_matrix((data, (row, col)), shape=(20, 65))

        # all processes check
        if self.rank == 0:
            self.assertTrue(((self.sp_A - sp_cb).todense() < 1e-7).all())
        self.assertTrue((sp_cb.todense() - D < 1e-7).all())
        # only root checks; owns matrix generated and subsequently written
        if self.rank == 0:
            self.assertTrue(((self.sp_A - C).todense() < 1e-7).all())

        #XXX: needs patched KDT.
        #FIXME: still crashes. Iterators are still interchanged.
        #SpParMat<long, doubleint, SpDCCols<long, doubleint>
        #>::SaveGathered<SpParMat<long, doubleint, SpDCCols<long, doubleint>
        #>>::ScalarReadSaveHandler> (this=this@entry=0x1cb6e70, filename=...,
        #handler=handler@entry=..., transpose=transpose@entry=false)
        # at kdt/pyCombBLAS/../../CombBLAS/SpParMat.cpp:1521
        # 1521 csr[nzit.rowid()].push_back( make_pair(colit.colid(),
        # nzit.value()) ); )
        #store.write(B)

        #B = store.read('combblas-sparse')
        #C = store.read('scipy-sparse')
        #D = store.read('numpy-dense')

        # convert CombBLAS matrix to (coo) sparse matrix
        #col, row, data = B.toVec()
        #sp_cb = scipy.sparse.coo_matrix((data, (row, col)), shape=(20, 65))
        #self.assertTrue(((self.sp_A - sp_cb).todense() < 1e-7).all())

        #self.assertTrue((self.sp_A.todense() - D < 1e-7).all())
        #self.assertTrue(((self.sp_A - C).todense() < 1e-7).all())

    def test_hdf5(self):
        fpath = 'test_matrix.h5'
        store = io.hdf5(fpath)

        # root writes its numpy-dense matrix...
        if self.rank == 0:
            store.write(self.np_A)
        MPI.COMM_WORLD.barrier()
        # ... all processes read back what root has written
        B = store.read('numpy-dense')
        C = store.read('elemental-dense')
        D = store.read('elemental-dense', distribution='VC_STAR')

        if self.rank == 0:
            self.assertTrue((self.np_A - B < 1e-7).all())
        self.compareMatrixNorm(C, self.np_A)
        self.compareMatrixNorm(D, self.np_A)

        # ... all proccesses write their part of (distributed) elemental-dense matrix
        store.write(self.ele_A)
        # ... and read back in various representations: local(copies) or distributed(parts)
        B = store.read('numpy-dense')
        C = store.read('elemental-dense')
        D = store.read('elemental-dense', distribution='VC_STAR')
        self.compareMatrixNorm(self.ele_A, B)
        self.compareMatrixNorm(C, B)
        self.compareMatrixNorm(D, B)

        # all unsupported read formats should raise an exception
        with self.assertRaises(io.SkylarkIOTypeError):
            store.read('combblas-sparse')
            store.read('scipy-sparse')

    def test_txt(self):
        fpath = 'test_matrix.txt'

        store = io.txt(fpath)
        store.write(self.ele_A)
        B = store.read('numpy-dense')
        self.compareMatrixNorm(self.ele_A, B)

        # root writes its numpy-dense matrix...
        if self.rank == 0:
            store.write(self.np_A)
        MPI.COMM_WORLD.barrier()
        # ... all processes read back what root has written
        B = store.read('numpy-dense')

        if self.rank == 0:
            self.assertTrue((self.np_A - B < 1e-7).all())

        # all unsupported read formats should raise an exception
        with self.assertRaises(io.SkylarkIOTypeError):
            store.read('elemental-dense')
            store.read('combblas-sparse')
            store.read('scipy-sparse')

    def test_libsvm(self):
        fpath = base_dir + '/python-skylark/skylark/datasets/usps.t'
        store = io.libsvm(fpath)
        features_matrix, labels_matrix = store.read()
        matrix_info = features_matrix.shape, features_matrix.nnz, labels_matrix.shape

        #FIXME: currently there is no way to test this in a sophisticated way.
        #       For now just test matrix_info
        self.assertEqual(matrix_info, ((2007, 256), 513792, (2007,)))


if __name__ == '__main__':
    import sys
    #XXX: hack to get argument to libsvm test
    global base_dir
    base_dir = sys.argv[1]
    del(sys.argv[1])
    unittest.main(verbosity=2)
