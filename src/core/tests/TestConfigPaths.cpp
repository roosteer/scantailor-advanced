// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <ConfigPaths.h>

#include <boost/test/unit_test.hpp>

namespace Tests {

BOOST_AUTO_TEST_SUITE(ConfigPathsTestSuite)

BOOST_AUTO_TEST_CASE(portable_root_owns_both_paths) {
  ConfigPaths::setPortableRoot(QStringLiteral("/tmp/st-test"));

  BOOST_CHECK(ConfigPaths::isPortable());
  BOOST_CHECK(ConfigPaths::configRoot() == QStringLiteral("/tmp/st-test"));
  BOOST_CHECK(ConfigPaths::profilesDir() == QStringLiteral("/tmp/st-test/profiles"));

  ConfigPaths::setPortableRoot(QString());
}

BOOST_AUTO_TEST_CASE(without_portable_root_paths_come_from_the_standard_location) {
  ConfigPaths::setPortableRoot(QString());

  BOOST_CHECK(!ConfigPaths::isPortable());
  BOOST_CHECK(ConfigPaths::profilesDir().endsWith(QStringLiteral("/profiles")));
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace Tests
