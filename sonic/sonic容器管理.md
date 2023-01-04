# sonic容器管理介绍

在sonic中，大量的服务以容器的形式提供，系统启动后，有system管理容器的运行。以下以swss容器为例介绍运行流程。

## system管理容器运行
    
system管理脚本在sonic-buildimage/files/build_templates/per_namespace/swss.service.j2
	编译过程中，该脚本会生成文件/lib/systemd/system/swss.service提供给systemd启动服务
	
    ```
	admin@sonic:/lib/systemd/system$ cat /lib/systemd/system/swss.service
	[Unit]
	Description=switch state service
	Requires=database.service
	After=database.service
	
	
	Requires=updategraph.service
	After=updategraph.service
	After=interfaces-config.service
	BindsTo=sonic.target
	After=sonic.target
	Before=ntp-config.service
	StartLimitIntervalSec=1200
	StartLimitBurst=3
	
	[Service]
	User=root
	Environment=sonic_asic_platform=vs
	ExecStartPre=/usr/local/bin/swss.sh start
	ExecStart=/usr/local/bin/swss.sh wait
	ExecStop=/usr/local/bin/swss.sh stop
	Restart=always
	RestartSec=30
	
	[Install]
	WantedBy=sonic.target
	
    ```

system命令介绍： 
```
systemctl status swss  #显示状态
systemd-analyze critical-chain lldp.service  # 显示启动依赖顺序
systemctl list-dependencies sonic.target #显示依赖状态
systemctl list-dependencies swss.target #同上，显示服务依赖状态
journalctl -u swss.service
```

sonic启动之后由systemd拉起服务swss.service：
```
	admin@sonic:/lib/systemd/system$ systemctl status swss
	● swss.service - switch state service
	     Loaded: loaded (/lib/systemd/system/swss.service; enabled; vendor preset: enabled)
	     Active: active (running) since Fri 2022-03-18 15:38:10 UTC; 3 days ago
	   Main PID: 1623 (swss.sh)
	      Tasks: 5 (limit: 7083)
	     Memory: 23.8M
	     CGroup: /system.slice/swss.service
	             ├─1623 /bin/bash /usr/local/bin/swss.sh wait
	             └─2184 python3 /usr/bin/docker-wait-any -s swss -d syncd teamd
```

/usr/local/bin/swss.sh内部调用/usr/bin/swss.sh创建容器 （该文件在./files/scripts/swss.sh）
```
	    # start service docker
	    /usr/bin/${SERVICE}.sh start $DEV
	    debug "Started ${SERVICE}$DEV service..."
```

容器创建脚本：
```
	 docker create --privileged -t -v /etc/network/interfaces:/etc/network/interfaces:ro -v /etc/network/interfaces.d/:/etc/network/interfaces.d/:ro -v /host/machine.conf:/host/machine.conf:ro -v /etc/sonic:/etc/sonic:ro -v /var/log/swss:/var/log/swss:rw  \
	        --net=$NET \
	        -e RUNTIME_OWNER=local \
	        --uts=host \
	        -v /src:/src:ro -v /debug:/debug:rw \
	        --log-opt max-size=2M --log-opt max-file=5 \
	        -e ASIC_VENDOR=vs \
	        -v /var/run/redis$DEV:/var/run/redis:rw \
	        -v /var/run/redis-chassis:/var/run/redis-chassis:ro \
	        -v /usr/share/sonic/device/$PLATFORM/$HWSKU/$DEV:/usr/share/sonic/hwsku:ro \
	        $REDIS_MNT \
	        -v /usr/share/sonic/device/$PLATFORM:/usr/share/sonic/platform:ro \
	        --tmpfs /tmp \
	        --tmpfs /var/tmp \
	        --env "NAMESPACE_ID"="$DEV" \
	        --env "NAMESPACE_PREFIX"="$NAMESPACE_PREFIX" \
	        --env "NAMESPACE_COUNT"=$NUM_ASIC \
	        --name=$DOCKERNAME docker-orchagent-dbg:latest || {
	            echo "Failed to docker run" >&1
	            exit 4
	    }
	
	        preStartAction
	       /usr/local/bin/container start ${DOCKERNAME}   #启动容器， 该脚本待分析
	       postStartAction
```	
container脚本中调用函数启动容器

## docker初始化

1. 在docker拉起容器时，执行脚本/usr/bin/docker-init.sh拉起服务。其中， 在docker构建过程中，通过DockerFile指定ENDPOINT。容器脚本相关文件在sonic-buildimages/dockers/docker-orchagent路径下
2. 在/usr/bin/docker-init.sh中，首先会创建配置路径，并通过sonic-cfg-gen组件生成配置
3. 最后，通过python进程管理攻击supervisord拉起各种服务。supervisor的介绍见：python 进程管理工具supervisor介绍

```
docker启动时运行如下脚本拉起服务 ：  docker-orchagent:latest   "/usr/bin/docker-init.sh" 

g22652@A12-Ubuntu:~/sonic-buildimage/dockers/docker-orchagent$ cat docker-init.sh
#!/usr/bin/env bash

mkdir -p /etc/swss/config.d/

CFGGEN_PARAMS=" \
    -d \
    -y /etc/sonic/constants.yml \
    -t /usr/share/sonic/templates/switch.json.j2,/etc/swss/config.d/switch.json \
    -t /usr/share/sonic/templates/ipinip.json.j2,/etc/swss/config.d/ipinip.json \
    -t /usr/share/sonic/templates/ports.json.j2,/etc/swss/config.d/ports.json \
    -t /usr/share/sonic/templates/vlan_vars.j2 \
    -t /usr/share/sonic/templates/ndppd.conf.j2,/etc/ndppd.conf \
"
VLAN=$(sonic-cfggen $CFGGEN_PARAMS)  #生成配置

# Executed HWSKU specific initialization tasks.
if [ -x /usr/share/sonic/hwsku/hwsku-init ]; then
    /usr/share/sonic/hwsku/hwsku-init
fi

# Start arp_update and NDP proxy daemon when VLAN exists
if [ "$VLAN" != "" ]; then
    cp /usr/share/sonic/templates/arp_update.conf /etc/supervisor/conf.d/
    cp /usr/share/sonic/templates/ndppd.conf /etc/supervisor/conf.d/
fi

exec /usr/local/bin/supervisord  #python 进程管理工具

```

## 背景架构
- systemd
- python supervisor