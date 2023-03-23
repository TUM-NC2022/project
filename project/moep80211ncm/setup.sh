if [ "$1" = "setupmoep" ]; then
    # Setup libmoep
    echo "Setup libmoep..."

    cd libmoep
    autoreconf -fi
    ./configure
    make
    make install
    ldconfig
    cd ..
fi

# Setup moep-ncm
echo "Setup moep80211-ncm..."

autoreconf -fi
./configure
make
