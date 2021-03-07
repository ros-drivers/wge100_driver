^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package wge100_camera
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1.8.3 (2021-03-07)
------------------
* removed deprecated test, updated CMakeLists.txt for new policy
* updated files for compile on focal/noetic
* updated changelogs
* fixed dependency issue with message generation in wge100_camera
* Contributors: Dave Feil-Seifer, David Feil-Seifer

* fixed dependency issue with message generation in wge100_camera
* Contributors: David Feil-Seifer

1.8.2 (2013-07-16)
------------------
* Fix install rule.
* Contributors: Austin Hendrix

1.8.1 (2013-07-14)
------------------
* Fix dependencies and set myself as maintainer.
* Contributors: Austin Hendrix

1.8.0 (2013-07-12)
------------------
* Version 1.8.0
* Finish catkinizing.
* Switching to catkin: wge100_camera now on catkin, but the CMakeLists is still incomplete. See TODOs.
* Contributors: Austin Hendrix, Georg Bartels

1.7.4 (2012-10-12)
------------------
* Fixed a linking issue with boost.
  Probably was implicitly provided by dependencies
  before, but since boost::thread is used directly
  by the node, it should be declared in this package.
* Adding .gitignore files
* Update for latest dynamic_reconfigure.
* remove roslib::Header usage, bump to 1.7.2
* Remove everything but WGE100 from wge100_driver stack.
* new stack for wge100 driver and firmware
* Contributors: Austin Hendrix, Brian Gerkey, Jack O'Quin, William Woodall
