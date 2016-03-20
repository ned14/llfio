#!/bin/bash
# Adapted from https://github.com/purpleKarrot/Karrot/blob/develop/test/coveralls.in
# which itself was adapted from https://github.com/berenm/cmake-extra/blob/master/coveralls-upload.in

if [ 0 -eq $(find -iname "*.gcda" | wc -l) ]
then
  exit 0
fi

gcov-4.8 --source-prefix $1 --preserve-paths --relative-only $(find -iname "*.gcda") 1>/dev/null || exit 0

cat >coverage.json <<EOF
{
  "service_name": "travis-ci",
  "service_job_id": "${TRAVIS_JOB_ID}",
  "run_at": "$(date --iso-8601=s)",
  "source_files": [
EOF

for file in $(find * -iname '*.gcov' -print | egrep '.*' | egrep -v 'valgrind|SpookyV2|bindlib|test')
do
  FILEPATH=$(echo ${file} | sed -re 's%#%\/%g; s%.gcov$%%')
  echo Reporting coverage for $FILEPATH ...
  cat >>coverage.json <<EOF
    {
      "name": "$FILEPATH",
      "source": $(cat $FILEPATH | python test/json_encode.py),
      "coverage": [$(tail -n +3 ${file} | cut -d ':' -f 1 | sed -re 's%^ +%%g; s%-%null%g; s%^[#=]+$%0%;' | tr $'\n' ',' | sed -re 's%,$%%')]
    },
EOF
done

#cat coverage.json
mv coverage.json coverage.json.tmp
cat >coverage.json <(head -n -1 coverage.json.tmp) <(echo -e "    }\n  ]\n}")
rm *.gcov coverage.json.tmp

#head coverage.json
#echo
curl -F json_file=@coverage.json https://coveralls.io/api/v1/jobs
#head coverage.json
