#!/bin/sh

# run from root folder, like `sh ./scripts/update_all.sh`
# as well as GNUMake (make) and python3

cd interface
rm -rf node_modules
corepack use pnpm@latest
pnpm update --latest
pnpm install
pnpm format
pnpm lint

cd ../mock-api
rm -rf node_modules
corepack use pnpm@latest
pnpm update --latest
pnpm install
pnpm format

cd ..
cd interface
pnpm build-webUI

cd ..
npx cspell "**"

# build files that go into docs folder
# platformio run -e build_modbus
# platformio run -e build_standalone

# run tests
# platformio run -e native-test -t exec
