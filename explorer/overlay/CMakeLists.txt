kcoreaddons_add_plugin(win12tagsoverlay SOURCES win12tagsoverlay.cpp INSTALL_NAMESPACE "kf6/overlayicon")
target_link_libraries(win12tagsoverlay win12tags KF6::KIOCore)
