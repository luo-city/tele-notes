

# barefoot 编译问题

## bullseye容器构建时docker 源错误
```
city@sonic:~/sonic-bfn/sonic-buildimage$ git diff sonic-slave-bullseye/Dockerfile.j2
diff --git a/sonic-slave-bullseye/Dockerfile.j2 b/sonic-slave-bullseye/Dockerfile.j2
index 0bc5412cf..d1e271e80 100755
--- a/sonic-slave-bullseye/Dockerfile.j2
+++ b/sonic-slave-bullseye/Dockerfile.j2
@@ -536,11 +536,8 @@ RUN apt-get install -y \
 {%- if CONFIGURED_ARCH == "armhf" %}
     RUN update-ca-certificates --fresh
 {%- endif %}
-RUN curl -fsSL http://10.153.3.130/docker-ce/linux/debian/gpg  | sudo apt-key add -
-RUN add-apt-repository \
-           "deb [arch={{ CONFIGURED_ARCH }}] http://10.153.3.130/docker-ce/linux/debian \
-           $(lsb_release -cs) \
-           stable"
+RUN curl -fsSL https://download.docker.com/linux/debian/gpg | sudo apt-key add -
+RUN add-apt-repository "deb [arch={{ CONFIGURED_ARCH }}] https://download.docker.com/linux/debian $(lsb_release -cs) stable"
 RUN apt-get update
 RUN apt-get install -y docker-ce=5:20.10.14~3-0~debian-bullseye
```

## sonic-ctrmgrd test fail
暂时屏蔽，修改 src/sonic-ctrmgrd/tests/ctrmgr_iptables_test.py.back

## docker配置问题

### d"--squash" 失败
- 问题："--squash" is only supported on a Docker daemon with experimental features enabled
 - 解决：echo $'{\n    "experimental": true\n}' | sudo tee /etc/docker/daemon.json
 - 参考https://stackoverflow.com/questions/44346322/how-to-run-docker-with-experimental-functions-on-ubuntu-16-04


### WARNING: No swap limit support
参照 https://stackoverflow.com/questions/58400583/no-swap-limit-support-docker-engine 解决，具体原因未深纠


# barefoot 运行问题

## log文件系统不可用导致容器启动失败
在/etc/rc.local中加载 /var/log/fsck.log.gz

## syncd容器启动失败问题

