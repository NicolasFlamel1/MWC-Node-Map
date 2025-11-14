# Program parameters
NAME = "MWC Node Map"
VERSION = "0.0.1"
CC = "g++"
STRIP = "strip"
CFLAGS = -I "./libmaxminddb/dist/include" -I "./libgit2/dist/include" -I "./blake2/include" -I "./secp256k1-zkp/dist/include" -I "./libzip/dist/include" -I "./croaring/dist/include" -static-libstdc++ -static-libgcc -O3 -Wall -Wextra -Wno-unknown-warning-option -Wno-vla -Wno-vla-cxx-extension -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unqualified-std-cast-call -std=c++23 -finput-charset=UTF-8 -fexec-charset=UTF-8 -funsigned-char -ffunction-sections -fdata-sections -DPROGRAM_NAME=$(NAME) -DPROGRAM_VERSION=$(VERSION) -DENABLE_TOR -DSET_DESIRED_NUMBER_OF_PEERS=32
LIBS = -L "./libmaxminddb/dist/lib" -L "./openssl/dist/lib" -L "./zlib/dist/lib" -L "./libgit2/dist/lib" -L "./secp256k1-zkp/dist/lib" -L "./libzip/dist/lib" -L "./croaring/dist/lib" -Wl,-Bstatic -lmaxminddb -lgit2 -lssl -lcrypto -lsecp256k1 -lzip -lz -lroaring -Wl,-Bdynamic -lpthread
SRCS = "./blake2/include/blake2b-ref.c" "./main.cpp" "./node/block.cpp" "./node/common.cpp" "./node/consensus.cpp" "./node/crypto.cpp" "./node/header.cpp" "./node/input.cpp" "./node/kernel.cpp" "./node/mempool.cpp" "./node/message.cpp" "./node/node.cpp" "./node/output.cpp" "./node/peer.cpp" "./node/proof_of_work.cpp" "./node/rangeproof.cpp" "./node/saturate_math.cpp" "./node/transaction.cpp"
PROGRAM_NAME = $(subst $\",,$(NAME))

# Check if using floonet
ifeq ($(FLOONET),1)

	# Enable floonet
	CFLAGS += -DENABLE_FLOONET
endif

# Make
all:
	$(CC) $(CFLAGS) -o "./$(PROGRAM_NAME)" $(SRCS) $(LIBS)
	$(STRIP) "./$(PROGRAM_NAME)"

# Make clean
clean:
	rm -rf "./$(PROGRAM_NAME)" "./libmaxminddb-1.12.2.tar.gz" "./libmaxminddb-1.12.2" "./libmaxminddb" "./openssl-3.3.0.tar.gz" "./openssl-3.3.0" "./openssl" "./zlib-1.3.1.tar.gz" "./zlib-1.3.1" "./zlib" "./v1.9.1.tar.gz" "./libgit2-1.9.1" "./libgit2" "./master.zip" "./BLAKE2-master" "./blake2" "./secp256k1-zkp-master" "./secp256k1-zkp" "./libzip-1.10.1.tar.gz" "./libzip-1.10.1" "./libzip" "./v4.0.0.zip" "./CRoaring-4.0.0" "./croaring" "./MWC-Validation-Node-master" "./node"

# Make run
run:
	"./$(PROGRAM_NAME)"

# Make install
install:
	rm -f "/usr/local/bin/$(PROGRAM_NAME)"
	mkdir -p "/usr/local/bin/"
	cp "./$(PROGRAM_NAME)" "/usr/local/bin/"
	chown root:root "/usr/local/bin/$(PROGRAM_NAME)"
	chmod 755 "/usr/local/bin/$(PROGRAM_NAME)"

# Make dependencies
dependencies:
	
	# Libmaxminddb
	wget "https://github.com/maxmind/libmaxminddb/releases/download/1.12.2/libmaxminddb-1.12.2.tar.gz"
	tar -xf "./libmaxminddb-1.12.2.tar.gz"
	rm "./libmaxminddb-1.12.2.tar.gz"
	mv "./libmaxminddb-1.12.2" "./libmaxminddb"
	cd "./libmaxminddb" && "./configure" --prefix="$(CURDIR)/libmaxminddb/dist" --disable-shared && make && make install
	
	# OpenSSL
	wget "https://github.com/openssl/openssl/releases/download/openssl-3.3.0/openssl-3.3.0.tar.gz"
	tar -xf "./openssl-3.3.0.tar.gz"
	rm "./openssl-3.3.0.tar.gz"
	mv "./openssl-3.3.0" "./openssl"
	cd "./openssl" && "./config" --prefix="$(CURDIR)/openssl/dist" --openssldir=$(shell openssl version -d | awk '{print $$2}') --libdir=lib --release no-shared && make && make install || true
	
	# Zlib
	wget "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz"
	tar -xf "./zlib-1.3.1.tar.gz"
	rm "./zlib-1.3.1.tar.gz"
	mv "./zlib-1.3.1" "./zlib"
	cd "./zlib" && "./configure" --prefix="$(CURDIR)/zlib/dist" --static && make && make install
	
	# Libgit2
	wget "https://github.com/libgit2/libgit2/archive/refs/tags/v1.9.1.tar.gz"
	tar -xf "./v1.9.1.tar.gz"
	rm "./v1.9.1.tar.gz"
	mv "./libgit2-1.9.1" "./libgit2"
	cd "./libgit2" && cmake -DCMAKE_INSTALL_PREFIX="$(CURDIR)/libgit2/dist" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DREGEX_BACKEND=builtin -DZLIB_INCLUDE_DIR="$(CURDIR)/zlib/dist/include" -DZLIB_LIBRARY="$(CURDIR)/zlib/dist/lib/libz.a" -DOPENSSL_INCLUDE_DIR="$(CURDIR)/openssl/dist/include" -DOPENSSL_SSL_LIBRARY="$(CURDIR)/openssl/dist/lib/libssl.a" -DOPENSSL_CRYPTO_LIBRARY="$(CURDIR)/openssl/dist/lib/libcrypto.a" "./CMakeLists.txt" && make && make install
	
	# BLAKE2
	wget "https://github.com/BLAKE2/BLAKE2/archive/master.zip"
	unzip "./master.zip"
	rm "./master.zip"
	mv "./BLAKE2-master" "./blake2"
	mv "./blake2/ref" "./blake2/include"
	
	# Secp256k1-zkp
	wget "https://github.com/mimblewimble/secp256k1-zkp/archive/refs/heads/master.zip"
	unzip "./master.zip"
	rm "./master.zip"
	mv "./secp256k1-zkp-master" "./secp256k1-zkp"
	cd "./secp256k1-zkp" && "./autogen.sh" && "./configure" --prefix="$(CURDIR)/secp256k1-zkp/dist" --disable-shared --enable-endomorphism --enable-experimental --enable-module-generator --enable-module-commitment --enable-module-rangeproof --enable-module-bulletproof --enable-module-aggsig --with-bignum=no --disable-benchmark && make && make install
	
	# Libzip
	wget "https://github.com/nih-at/libzip/releases/download/v1.10.1/libzip-1.10.1.tar.gz"
	tar -xf "./libzip-1.10.1.tar.gz"
	rm "./libzip-1.10.1.tar.gz"
	mv "./libzip-1.10.1" "./libzip"
	cd "./libzip" && cmake -DCMAKE_INSTALL_PREFIX="$(CURDIR)/libzip/dist" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DZLIB_INCLUDE_DIR="$(CURDIR)/zlib/dist/include" -DZLIB_LIBRARY="$(CURDIR)/zlib/dist/lib/libz.a" -DENABLE_BZIP2=OFF -DENABLE_ZSTD=OFF -DENABLE_LZMA=OFF -DENABLE_OPENSSL=OFF "./CMakeLists.txt" && make && make install
	
	# CRoaring
	wget "https://github.com/RoaringBitmap/CRoaring/archive/refs/tags/v4.0.0.zip"
	unzip "./v4.0.0.zip"
	rm "./v4.0.0.zip"
	mv "./CRoaring-4.0.0" "./croaring"
	cd "./croaring" && cmake -DCMAKE_INSTALL_PREFIX="$(CURDIR)/croaring/dist" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DENABLE_ROARING_TESTS=OFF "./CMakeLists.txt" && make && make install
	
	# MWC Validation Node
	wget "https://github.com/NicolasFlamel1/MWC-Validation-Node/archive/refs/heads/master.zip"
	unzip "./master.zip"
	rm "./master.zip"
	mv "./MWC-Validation-Node-master" "./node"

# Make IP geolocate database
ipGeolocateDatabase:
	
	# IP geolocate database provided by DB-IP (https://db-ip.com)
	wget -q -O - "https://db-ip.com/db/download/ip-to-city-lite" | grep -o "https:\/\/download\.db-ip\.com\/free\/dbip-city-lite-.*\?\.mmdb\.gz" | wget -q -i - -O - | gzip -d > "./ip_geolocate_database.mmdb"
