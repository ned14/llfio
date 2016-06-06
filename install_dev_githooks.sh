#!/bin/sh
echo Installing post-receive hook which recursively forces all
echo submodules to latest commit after every git pull
cat <<EOT >> .git/hooks/post-merge
#!/bin/sh
git submodule foreach --recursive 'branch=\$(git config -f \$toplevel/.gitmodules submodule.\$name.branch); [ \"\$branch\" = \"\" ] && branch=master; git checkout \$branch; git merge origin/$branch;'
EOT
chmod +x .git/hooks/post-merge
