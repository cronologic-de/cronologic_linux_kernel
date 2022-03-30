#
# Conan recipe used to create the package of "headers" the kernel module 
# for Linux, from the package `cronologic_linux_kernel`.
# It packages both the source code and the generated binary kernel module. 
#
from conans import ConanFile

class CronoLinuxKerneModuleHeadersConan(ConanFile):
    python_requires = "crono_utils/0.0.1"
    python_requires_extend = "crono_utils.CronoConanBase"

    # __________________________________________________________________________
    # Values to be reviewed with every new version
    #
    version = "0.0.1"

    # __________________________________________________________________________
    # Member variables
    #
    drvmod_pkg_name = "cronologic_linux_kernel"
    drvmod_ref = drvmod_pkg_name + "/" + version
    name = drvmod_pkg_name + "-headers"

    license = "GPL-3.0 License"
    author = "Bassem Ramzy <SanPatBRS@gmail.com>"
    url = "https://conan.cronologic.de/artifactory/prod/_/_/cronologic_linux_kernel-headers/"	\
          + version
    description = "Cronologic linux kernel module header file(s)"
    topics = ["cronologic", "pci", "kernel", "module", "linux"]
    settings = ["os", "compiler", "build_type", "arch", "distro"]

    # `CronoConanBase` variables initialization and export
    supported_os = ["Linux"]
    proj_src_indir = "../.."
    is_headers = True
