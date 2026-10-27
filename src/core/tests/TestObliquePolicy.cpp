// Copyright (C) 2019  Joseph Artsimovich <joseph.artsimovich@gmail.com>, 4lex4 <4lex49@zoho.com>
// Use of this source code is governed by the GNU GPLv3 license that can be found in the LICENSE file.

#include <filters/deskew/ObliquePolicy.h>

#include <boost/test/unit_test.hpp>

namespace Tests {

BOOST_AUTO_TEST_SUITE(ObliquePolicyTestSuite)

BOOST_AUTO_TEST_CASE(explicit_request_wins_over_the_profile_default) {
  BOOST_CHECK(deskew::resolveAutoOblique(true, false));
  BOOST_CHECK(!deskew::resolveAutoOblique(false, true));
}

BOOST_AUTO_TEST_CASE(without_a_request_the_profile_default_decides) {
  BOOST_CHECK(deskew::resolveAutoOblique(std::nullopt, true));
  BOOST_CHECK(!deskew::resolveAutoOblique(std::nullopt, false));
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace Tests
