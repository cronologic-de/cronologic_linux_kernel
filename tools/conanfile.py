#
# Conan recipe used to create the package of the kernel module for Linux.
# It packages both the source code and the generated binary kernel module. 
# It uses the projects' `CMake` whenever a build is needed.
#
from conans import ConanFile

class CronoLinuxKerneModuleConan(ConanFile):
    python_requires = "crono_utils/0.0.1"
    python_requires_extend = "crono_utils.CronoConanBase"

    # __________________________________________________________________________
    # Values to be reviewed with every new version
    #
    version = "0.0.1"

    # __________________________________________________________________________
    # Member variables
    #
    name = "cronologic_linux_kernel"
    license = "GPL-3.0 License"
    author = "Bassem Ramzy <SanPatBRS@gmail.com>"
    url = "https://conan.cronologic.de/artifactory/prod/_/_/cronologic_linux_kernel/"	\
          + version
    description = "Cronologic PCI Kernel Module"
    topics = ["cronologic", "pci", "kernel", "module", "linux"]
    settings = ["os", "compiler", "build_type", "arch", "distro"]

    # `CronoConanBase` variables initialization and export
    supported_os = ["Linux"]
    proj_src_indir = ".."
    export_source = True

    # ==========================================================================
    # Conan Methods
    #
    def package(self):
        super().package(exec_name="crono_pci_drvmod.ko")
        
    # __________________________________________________________________________
