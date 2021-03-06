#! /bin/tcsh -f

if ( ! -e ${FREESURFER_HOME}/MCRv80/bin/glnxa64/libdctprocess.so \
  && ! -e ${FREESURFER_HOME}/MCRv80/bin/maci64/libdctprocess.jnilib  ) then
  echo "     "
  echo "ERROR: cannot find Matlab 2012b runtime in location:"
  echo "     "
  echo "${FREESURFER_HOME}/MCRv80"
  echo "     "
  echo "It is looking for either: "
  echo "  bin/glnxa64/libdctprocess.so    (Linux 64b) or"
  echo "  bin/maci64/libdctprocess.jnilib (Mac 64b)"
  echo " "
  echo "The hippocampal/amygdala and brainstem modules require the (free) Matlab runtime." 
  echo "You will need to download the Matlab Compiler Runtime (MCR) for Matlab 2012b." 
  echo "To do so, please run the following commands (you might need root permissions):"
  echo " "
  echo "LINUX:"
  echo " "
  echo "cd ${FREESURFER_HOME}"
  echo "curl "\""http://surfer.nmr.mgh.harvard.edu/fswiki/MatlabRuntime?action=AttachFile&do=get&target=runtime2012bLinux.tar.gz"\"" -o "\""runtime2012b.tar.gz"\"""
  echo "tar xvf runtime2012b.tar.gz"
  echo " "
  echo "MAC:"
  echo " "
  echo "cd ${FREESURFER_HOME}"
  echo "curl "\""http://surfer.nmr.mgh.harvard.edu/fswiki/MatlabRuntime?action=AttachFile&do=get&target=runtime2012bMAC.tar.gz"\"" -o "\""runtime2012b.tar.gz"\"""
  echo "tar xvf runtime2012b.tar.gz"
  echo " "
  echo " "
  echo "For further details, please visit:"
  echo "     "
  echo "https://surfer.nmr.mgh.harvard.edu/fswiki/MatlabRuntime"
  echo " "
  exit 1
endif

exit 0



