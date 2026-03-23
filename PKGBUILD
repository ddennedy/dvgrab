# Maintainer: Dan Dennedy <dan@dennedy.org>
pkgname=dvgrab
pkgver=3.5.2
pkgrel=1
pkgdesc="Grab digital video data via a FireWire (IEEE 1394) or USB connection"
arch=('x86_64')
url="https://github.com/ddennedy/dvgrab"
license=('GPL-2.0-only')
depends=('libraw1394' 'libavc1394' 'libiec61883')
optdepends=(
  'libdv: JPEG output support for DV video'
  'libjpeg-turbo: JPEG frame extraction'
)
makedepends=('autoconf' 'automake')
source=("${pkgname}-${pkgver}.tar.gz::https://github.com/ddennedy/dvgrab/archive/v${pkgver}.tar.gz")
sha256sums=('d5558cd419c8d46bdc958064cb97f963d1ea793866414c025906ec15033512ed')

build() {
  cd "${pkgname}-${pkgver}"
  ./bootstrap
  ./configure --prefix=/usr
  make
}

package() {
  cd "${pkgname}-${pkgver}"
  make DESTDIR="${pkgdir}" install
}
