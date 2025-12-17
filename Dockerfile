FROM ghcr.io/wiiu-env/devkitppc:20241128

# Copy WUPS (Wii U Plugin System), libnotifications, and libmappedmemory from their artifact images
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:latest /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:latest /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libmappedmemory:20230621 /artifacts $DEVKITPRO

# Install libgd and dependencies for image loading
# Retry with backoff - devkitPro servers sometimes rate-limit CI runners
RUN for i in 1 2 3 4 5; do \
      dkp-pacman -Sy --noconfirm --overwrite '*' ppc-libgd ppc-libpng ppc-libjpeg-turbo ppc-zlib && break || \
      echo "Attempt $i failed, retrying in $((i*10)) seconds..." && sleep $((i*10)); \
    done

COPY --chown=developer:developer . /project
WORKDIR /project
RUN make -j$(nproc)
