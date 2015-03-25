BF_BUILDDIR = '../blender-build/linux-glibc211-x86_64'
BF_INSTALLDIR = '../blender-install/linux-glibc211-x86_64'
BF_NUMJOBS = 1

# XXX disabled cuda bins for gooseberry branch for faster builds
#BF_CYCLES_CUDA_BINARIES_ARCH = ['sm_20', 'sm_21', 'sm_30', 'sm_35', 'sm_50', 'sm_52']
BF_CYCLES_CUDA_BINARIES_ARCH = []
