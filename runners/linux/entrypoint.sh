#!/usr/bin/env /bin/bash
if ([ -d /runner ] && [ "$(ls -A /runner 2>/dev/null)" = "" ]) || [ ! -f /runner/.credentials ]; then
    cp -r * /runner

    cd /runner
    RUNNER_ALLOW_RUNASROOT=1 ./config.sh --unattended --url $REPO --token $TOKEN --name linux
else
    cd /runner
fi

RUNNER_ALLOW_RUNASROOT=1 ./run.sh