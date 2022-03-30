#
# Conan recipe used to create the package of "binary" the kernel module 
# for Linux, from the package `cronologic_linux_kernel`.
# It packages both the source code and the generated binary kernel module. 
# It uses the projects' `CMake` whenever a build is needed.
#
from conans import ConanFile

class CronoLinuxKerneModuleBinConan(ConanFile):
    python_requires = "crono_utils/0.0.1"
    python_requires_extend = "crono_utils.CronoConanBase"

    # __________________________________________________________________________
    # Values to be reviewed with every new version
    #
    # Version MUST BE equal to the same version of Main Package of the source 
    # code used to build the binaries.
    version = "0.0.1"

    # __________________________________________________________________________
    # Member variables
    #
    name = "cronologic_linux_kernel-bin"

    license = "GPL-3.0 License"
    author = "Bassem Ramzy <SanPatBRS@gmail.com>"
    url = "https://conan.cronologic.de/artifactory/prod/_/_/cronologic_linux_kernel-bin/" \
        + version
    description = "Cronologic kernel module binary file(s)"
    topics = ["cronologic", "pci", "kernel", "module", "linux"]
    settings = ["os", "compiler", "build_type", "arch", "distro"]

    # `CronoConanBase` variables initialization
    supported_os = ["Linux"]
    proj_src_indir = "../.."
    is_bin = True

    # ==========================================================================
    # Conan Methods
    #
    def package(self):
        super().package(exec_name="crono_pci_drvmod.ko")

    # __________________________________________________________________________
