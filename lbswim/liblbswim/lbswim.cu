// -*- mode: C++ -*-
// This is the combined CUDA source file to escape linking difficulties
#include "SharedItem.hpp"
#include "SharedArray.hpp"

#include "Lattice.cu"
#include "SwimmerArray.cu"
#include "TracerArray.cu"

template class SharedItem<ScalarList>;
template class SharedItem<VectorList>;
template class SharedItem<ScalarField>;
template class SharedItem<VectorField>;
template class SharedItem<DistField>;
template class SharedItem<CommonParams>;
