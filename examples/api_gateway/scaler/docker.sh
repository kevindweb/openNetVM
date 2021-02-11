#!/bin/bash
while getopts :i:c: OPTION ; do
    case ${OPTION} in
        i) ID="${OPTARG}" ;;
        c) CMD=${OPTARG} ;;
        \?) echo "Unknown option -$OPTARG" && exit 1
    esac
done

NAME="skeleton${ID}"
CMD="./container/test.sh"

if [[ "${ID}" == "" ]] || [[ "${CMD}" == "" ]] ; then
    echo -e "Need to supply correct arguments"
    exit 1
fi

sudo docker run \
    --name="${NAME}" \
    --hostname="${NAME}" \
    --volume=/tmp/container/${ID}:/tmp/pipe \
    --volume=/local/onvm/openNetVM/container:/container \
    --detach \
    ubuntu:20.04 \
    /bin/bash -c "${CMD}"
    # /bin/bash

# exit with docker error code (0 on success)
exit $?