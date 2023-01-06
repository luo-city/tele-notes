## 问题1， 容器启动失败，导致无法进入kdb调试
1. 修改配置./platform/barefoot/docker-syncd-bfn/supervisord.conf
   删除监听syncd进程启动失败时退出部分，使得syncd启动失败后容器不会退出，后续可手动拉起容器调试
2. 重新编译容器 make target/docker-syncd-bfn-dbg,gz
3. 将编译后的容器上传设备，并执行docker image load -i docker-syncd-bfn-dbg
4. 重启syncd容器，开始syncd调试

## 问题2， syncd启动（/usr/bin/syncd -u -s -l -p /tmp/sai.profile）报错
```
+ exec /usr/bin/syncd -u -s -l -p /tmp/sai.profile
bf_switchd: Install dir: /opt/bfn/install
bf_switchd: system services initialized
bf_switchd: loading conf_file /opt/bfn/install/share/p4/targets/tofino/switch-sai.conf...
bf_switchd: processing device configuration...
Configuration for dev_id 0
  Family        : Tofino
  pci_sysfs_str :
  pci_int_mode  : 0
  sds_fw_path   : share/tofino_sds_fw/avago/firmware
bf_switchd: processing P4 configuration...
P4 profile for dev_id 0
num P4 programs 1
  p4_name: switch
  p4_pipeline_name: pipe
    libpd:
    libpdthrift:
    context: /opt/bfn/install/share/switch/pipe/context.json
    config: /opt/bfn/install/share/switch/pipe/tofino.bin
  diag:
  accton diag:
  Agent[0]: /opt/bfn/install/lib/libpltfm_mgr.so
bf_switchd: library /opt/bfn/install/lib/libsai.so loaded
Operational mode set to default: MODEL
Starting PD-API RPC server on port 9090
bf_switchd: drivers initialized
bf_switchd: initializing dru_sim service
ERROR:bf_switchd_lib_load:2647: dlopen failed for libdru_sim.so, err=libdru_sim.so: cannot open shared object file: No such file or directory
ERROR: Unable to load dru sim library
ERROR: dru_sim initialization failed : -1
Segmentation fault (core dumped)

```

在打包sdk时没有包含libdru_sim.so， 具体流程待确认。
临时修改方案：修改make_sde_deb.sh， 将libdru_sim.so打包