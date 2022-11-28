# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if (NOT CONFIG_COVERAGE_GCOV)
  set_property(TARGET linker PROPERTY coverage -lgcov)
endif()

# GCC 11 by default emits DWARF version 5 which cannot be parsed by
# pyelftools. Can be removed once pyelftools supports v5.
add_link_options(-gdwarf-4)
