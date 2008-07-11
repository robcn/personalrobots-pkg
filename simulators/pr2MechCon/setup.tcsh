#!/bin/tcsh
setenv GAZ_TOP `rospack find gazebo`/gazebo
setenv OGRE_TOP `rospack find ogre`/ogre
setenv FREEIMAGE_TOP `rospack find freeimage`/freeimage
setenv CG_TOP `rospack find Cg`/Cg
setenv SIM_PLUGIN `rospack find gazebo_plugin`
setenv PR2API `rospack find libpr2API`
setenv PR2HW `rospack find libpr2HW`
setenv PR2MEDIA `rospack find 2dnav-gazebo`/world
setenv ROSTF `rospack find rosTF`
setenv LIBTF `rospack find libTF`
setenv ROSCPP `rospack find roscpp`
setenv ROSTH `rospack find rosthread`
setenv XMLRPC `rospack find xmlrpc++`

if (! $?LD_LIBRARY_PATH) setenv LD_LIBRARY_PATH ''
setenv LD_LIBRARY_PATH ''
setenv LD_LIBRARY_PATH $PR2API/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $PR2HW/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $SIM_PLUGIN/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $GAZ_TOP/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $OGRE_TOP/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $OGRE_TOP/lib/OGRE:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $FREEIMAGE_TOP/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $CG_TOP/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $SIM_PLUGIN/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $ROSTF/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $LIBTF/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $ROSCPP/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $ROSTH/lib:$LD_LIBRARY_PATH
setenv LD_LIBRARY_PATH $XMLRPC/lib:$LD_LIBRARY_PATH
setenv PATH $GAZ_TOP/bin:$PATH

setenv GAZEBO_RESOURCE_PATH $PR2MEDIA
setenv OGRE_RESOURCE_PATH $OGRE_TOP/lib/OGRE

echo
echo Current GAZ_TOP is set to $GAZ_TOP
echo Paths have been set accordingly.
echo
echo Now run gazebo [...world file...]
echo

