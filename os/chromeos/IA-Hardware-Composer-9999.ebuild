# Copyright 2016 Intel Corporation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5
inherit cros-workon eutils autotools

CROS_WORKON_PROJECT="chromiumos/third_party/IA-Hardware-Composer"
KEYWORDS="~x86 ~amd64"

RESTRICT="mirror"

DESCRIPTION="test for hardware composer"
HOMEPAGE=""

LICENSE="apache"
SLOT="0"

RDEPEND="${DEPEND}"

src_prepare() {
    eautoreconf
}

src_configure() {
    econf
}

src_compile() {
	emake
}

src_install() {
	dobin tests/testlayers
}
