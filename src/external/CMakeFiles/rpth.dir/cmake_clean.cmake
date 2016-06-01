FILE(REMOVE_RECURSE
  "CMakeFiles/rpth"
  "CMakeFiles/rpth-complete"
  "rpth/src/rpth-stamp/rpth-install"
  "rpth/src/rpth-stamp/rpth-mkdir"
  "rpth/src/rpth-stamp/rpth-download"
  "rpth/src/rpth-stamp/rpth-update"
  "rpth/src/rpth-stamp/rpth-patch"
  "rpth/src/rpth-stamp/rpth-configure"
  "rpth/src/rpth-stamp/rpth-build"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/rpth.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
