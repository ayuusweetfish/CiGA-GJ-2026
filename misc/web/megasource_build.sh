MEGASOURCE=${1:-$PWD}
J=${J:-1}
echo MEGASOURCE=${MEGASOURCE}
echo J=${J}

(
  cd ${MEGASOURCE}
  mkdir -p build/release build/compat

  false && (
    cd build/release
    emcmake cmake ${MEGASOURCE} -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DLOVE_JIT=0 -DCMAKE_BUILD_TYPE=Release
    emmake make -j${J}
    # love/love.js* love/love.wasm love/love.worker.js
  )

  (
    cd build/compat
    emcmake cmake ${MEGASOURCE} -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DLOVE_JIT=0 -DCMAKE_BUILD_TYPE=Release -DLOVEJS_COMPAT=1 -DSEXPORT_ALL=1 -DSMAIN_MODULE=1 -DSERROR_ON_UNDEFINED_SYMBOLS=0
    emmake make -j${J}
    # love/love.js* cp love/love.wasm
  )
)
