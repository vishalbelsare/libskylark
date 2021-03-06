#ifndef SKYLARK_INNER_HPP
#define SKYLARK_INNER_HPP

#include <boost/mpi.hpp>

namespace skylark { namespace base {

template<typename T>
inline El::Base<T> Nrm2(const El::Matrix<T>& x) {
    return El::Nrm2(x);
}

template<typename T>
inline El::Base<T> Nrm2(const El::DistMatrix<T>& x) {
    return El::Nrm2(x);
}

template<typename T>
inline El::Base<T> Nrm2(const El::DistMatrix<T, El::VC, El::STAR>& x) {
    boost::mpi::communicator comm(x.DistComm().comm, boost::mpi::comm_attach);
    T local = El::Nrm2(x.LockedMatrix());
    T snrm = boost::mpi::all_reduce(comm, local * local, std::plus<T>());
    return sqrt(snrm);
}

template<typename T>
inline El::Base<T> Nrm2(const El::DistMatrix<T, El::VR, El::STAR>& x) {
    boost::mpi::communicator comm(x.DistComm(), boost::mpi::comm_attach);
    T local = El::Nrm2(x.LockedMatrix());
    T snrm = boost::mpi::all_reduce(comm, local * local, std::plus<T>());
    return sqrt(snrm);
}

template<typename T>
inline El::Base<T> Nrm2(const El::DistMatrix<T, El::STAR, El::STAR>& x) {
    return El::Nrm2(x.LockedMatrix());
}

template<typename T>
inline void ColumnNrm2(const El::Matrix<T>& A,
    El::Matrix<El::Base<T> >& N) {

    N.Resize(A.Width(), 1);
    T *n = N.Buffer();
    const T *a = A.LockedBuffer();
    for(El::Int j = 0; j < A.Width(); j++) {
        n[j] = 0.0;
        for(El::Int i = 0; i < A.Height(); i++)
            n[j] += a[j * A.LDim() + i] * El::Conj(a[j * A.LDim() + i]);
        n[j] = sqrt(n[j]);
    }
}

template<typename T>
inline void ColumnNrm2(const El::DistMatrix<T, El::STAR, El::STAR>& A,
    El::DistMatrix<El::Base<T>, El::STAR, El::STAR>& N) {
    N.Resize(A.Width(), 1);
    T *n = N.Buffer();
    const T *a = A.LockedBuffer();
    for(El::Int j = 0; j < A.Width(); j++) {
        n[j] = 0.0;
        for(El::Int i = 0; i < A.LocalHeight(); i++)
            n[j] += a[j * A.LDim() + i] * El::Conj(a[j * A.LDim() + i]);
        n[j] = sqrt(n[j]);
    }
}

template<typename T, El::Distribution U, El::Distribution V>
inline void ColumnNrm2(const El::DistMatrix<T, U, V>& A,
    El::DistMatrix<El::Base<T>, El::STAR, El::STAR>& N) {

    std::vector<T> n(A.Width(), 1);
    std::fill(n.begin(), n.end(), 0.0);
    const El::Matrix<T> &Al = A.LockedMatrix();
    const T *a = Al.LockedBuffer();
    for(int j = 0; j < Al.Width(); j++)
        for(int i = 0; i < Al.Height(); i++)
            n[A.GlobalCol(j)] +=
                a[j * Al.LDim() + i] * El::Conj(a[j * Al.LDim() + i]);

    N.Resize(A.Width(), 1);
    El::Zero(N);
    boost::mpi::communicator comm(N.Grid().Comm().comm, boost::mpi::comm_attach);
    boost::mpi::all_reduce(comm, n.data(), A.Width(), N.Buffer(), std::plus<T>());
    for(int j = 0; j < A.Width(); j++)
        N.Set(j, 0, sqrt(N.Get(j, 0)));
}

template<typename T>
inline void ColumnNrm2(const El::AbstractDistMatrix<T>& A,
    El::DistMatrix<El::Base<T>, El::STAR, El::STAR>& N) {

    N.Resize(A.Width(), 1);

    if (A.Participating()) {
        std::vector<T> n(A.Width(), 1);
        std::fill(n.begin(), n.end(), 0.0);
        const El::Matrix<T> &Al = A.LockedMatrix();
        const T *a = Al.LockedBuffer();
        for(El::Int j = 0; j < Al.Width(); j++)
            for(El::Int i = 0; i < Al.Height(); i++)
                n[A.GlobalCol(j)] +=
                    a[j * Al.LDim() + i] * El::Conj(a[j * Al.LDim() + i]);


        El::Zero(N);
        El::mpi::AllReduce(n.data(), N.Buffer(), A.Width(), MPI_SUM,
            A.DistComm());
        for(El::Int j = 0; j < A.Width(); j++)
            N.Set(j, 0, sqrt(N.Get(j, 0)));
    }

    El::mpi::Broadcast(N.Buffer(), A.Width(), A.Root(), A.CrossComm());
}


template<typename T>
inline void ColumnDot(const El::Matrix<T>& A, const El::Matrix<T>& B,
    El::Matrix<T>& N) {

    // TODO just assuming sizes are OK for now.

    T *n = N.Buffer();
    const T *a = A.LockedBuffer();
    const T *b = B.LockedBuffer();
    for(El::Int j = 0; j < A.Width(); j++) {
        n[j] = 0.0;
        for(El::Int i = 0; i < A.Height(); i++)
            n[j] += a[j * A.LDim() + i] * El::Conj(b[j * B.LDim() + i]);
    }
}

template<typename T>
inline void ColumnDot(const El::DistMatrix<T, El::STAR, El::STAR>& A,
    const El::DistMatrix<T, El::STAR, El::STAR>& B,
    El::DistMatrix<T, El::STAR, El::STAR>& N) {

    // TODO just assuming sizes are OK for now.

    T *n = N.Buffer();
    const T *a = A.LockedBuffer();
    const T *b = B.LockedBuffer();
    for(El::Int j = 0; j < A.Width(); j++) {
        n[j] = 0.0;
        for(El::Int i = 0; i < A.LocalHeight(); i++)
            n[j] += a[j * A.LDim() + i] * El::Conj(b[j * B.LDim() + i]);
    }
}

template<typename T, El::Distribution U, El::Distribution V>
inline void ColumnDot(const El::DistMatrix<T, U, V>& A,
    const El::DistMatrix<T, U, V>& B,
    El::DistMatrix<T, El::STAR, El::STAR>& N) {

    // TODO just assuming sizes are OK for now, and grid aligned.
    std::vector<T> n(A.Width(), 1);
    std::fill(n.begin(), n.end(), 0);
    const El::Matrix<T> &Al = A.LockedMatrix();
    const T *a = Al.LockedBuffer();
    const El::Matrix<T> &Bl = B.LockedMatrix();
    const T *b = Bl.LockedBuffer();
    for(El::Int j = 0; j < Al.Width(); j++)
        for(El::Int i = 0; i < Al.Height(); i++)
            n[A.GlobalCol(j)] +=
                a[j * Al.LDim() + i] * El::Conj(b[j * Bl.LDim() + i]);

   N.Resize(A.Width(), 1);
   El::Zero(N);
   boost::mpi::communicator comm(N.Grid().Comm().comm, boost::mpi::comm_attach);
   boost::mpi::all_reduce(comm, n.data(), A.Width(), N.Buffer(), std::plus<T>());
}

template<typename T>
inline void RowDot(const El::Matrix<T>& A, const El::Matrix<T>& B, 
    El::Matrix<T>& N) {

    // TODO just assuming sizes are OK for now.

    T *n = N.Buffer();
    const T *a = A.LockedBuffer();
    const T *b = B.LockedBuffer();
    for(El::Int i = 0; i < A.Height(); i++)
        n[i] = 0.0;

    for(El::Int j = 0; j < A.Width(); j++) {
        for(El::Int i = 0; i < A.Height(); i++)
            n[i] += a[j * A.LDim() + i] * El::Conj(b[j * B.LDim() + i]);
    }
}

} } // namespace skylark::base

#endif // SKYLARK_INNER_HPP
