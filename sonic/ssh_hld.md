# SSH 服务 设计文档

## 概述
在sonic中，使用open_ssh对外提供ssh服务，终端用户可通过ssh连接登陆到sonic设备并远程控制。当用户通过ssh登陆时，需要配置ssh服务，控制用户登陆权限。但是sonic没有封装提供对ssh配置的config命令，需要手动修改sshd_config等配置文件，因此，针对字节需求，增加ssh配置封装config命令，用于修改ssh对应的配置文件，显示当前配置信息，显示日志等功能。

sonic当前使用的是OpenSSH_8.2p1版本，具体ssh的版本信息可查看openssh[官方文档](http://www.openssh.com/)获取。

在字节一期的需求中，有如下功能需要支持： 

- 配置ssh服务使能/去使能
- 配置ssh监听端口
- 配置ssh监听的IP地址
- 支持IPv6登陆，并支持配置ipv6监听的端口和地址
- 配置允许登陆/拒绝登陆的IP地址
- 配置同时在线的最大session数量
- 配置最大错误验证次数
- 配置idle-time时间
- 支持SFTP client
- 显示当前配置信息
- 显示用户登陆日志信息
- 显示当前在线用户信息

## 配置/显示命令
在sonic-utilities项目中，新增config ssh命令和show ssh命令，用于封装对应的配置文件修改、配置显示等内容，sonic命令行开发、bash自动补齐、命令行自动化测试用例请参见sonic-utilities项目

## sshd_config文件说明
sshd从/etc/ssh/sshd_config文件读取配置，如果在执行是指定-f参数，则从指定的配置文件中读取配置。sshd_config文件使用key-value对表示参数，#开头或者空行表示注释，如果参数中间有空格，需要使用双引号包含，更加详细的说明[sshd_config](https://man.openbsd.org/sshd_config), 或者[博客](https://blog.csdn.net/zhu_xun/article/details/18304441). 这里只列举我们需要使用的和一些关键的参数：
- AddressFamily 监听地址族，缺省为any，可指定inet（只监听IPv4）或者inet6（只监听ipv6），使用缺省，不修改
- ListenAddress 指定监听的网络地址，默认监听所有的地址。可以使用多个ListenAddress指定多个地址。
- Port 指定监听的端口号，默认为22，可以通过多条指令监听多个端口
- ClientAliveCountMax 在未收到客户端回应前最多允许发送多少"alive"
- ClientAliveInterval 设置一个以秒计的市场，超时后发送alive并等待应答
- MaxAuthTries 指定每个连接最大允许的认证次数。默认值是6。

## 分析说明
在sonic中，sshd的配置放在以下文件：
- /etc/ssh/sshd_config文件，存放主要配置文件
- /etc/hosts_allow, /etc/hosts_deny, 存放上本机IP地址配置信息，可指定服务，类似ssh:192.168.249.211:allow
- /etc/security/limits.conf, 登陆用户数量限制。有pam模块管理。
  
考虑到如果配置信息存在在数据库，还需要管理配置文件和数据库的同步，因此，只把ssh的配置数据保存在配置文件中。封装sshd命令，实现对配置文件的解析和更新。具体实现可使用shell或者python均可。


## 命令行设计
### 配置ssh服务使能/去使能/重启
config ssh { disable | enable } 

实现逻辑：封装命令执行systemctl stop/start sshd

### 配置ssh监听IP地址和端口
config ssh listenAddress ip {add|remove}  x.x.x.x

config ssh listenAddress ip {add|remove}  x.x.x.x  -o port

config ssh listenAddress ip {add|remove}  x:x:x:x

config ssh listenAddress ip {add|remove}   x:x:x:x  -o port

config ssh listenPort {add|remove} xx

实现逻辑：查找/etc/ssh/sshd_config文件 ListenAddress/Port对应项，并添加或者删除，注意重复配置
被占用的端口和没有配置的IP地址不能配置成功

### 配置允许登陆的IP地址
config ssh { allowAddress | denyAddress } {add|remove} { x.x.x.x  | all  | x::x:x:x  | x.x.x.* | x.x.x.0 -p xx| x:: -p xx}

sshd: 2::100

sshd: 2::   1 

sshd:192.168.249.27

sshd:192.168.249.*

sshd:192.168.249.0  24

sshd:all

实现逻辑：解析hosts.allow和hosts.deny,添加删除对应记录
检查策略是先看/etc/hosts.allow中是否允许，如果允许直接放行；如果没有，则再看/etc/hosts.deny中是否禁止，如果禁止那么就禁止连入

### 配置同时在线的最大session数量
config ssh maxLogins default|xx

实现逻辑：解析修改/etc/security/limits.conf行*                -   maxsyslogins     3


### 配置最大错误验证次数
config ssh maxAuthTries default|xx

实现逻辑：解析修改/etc/ssh/sshd_config文件 MaxAuthTries

### 配置idle-time时间
config ssh session-idle-time default|xx  (分钟计)

实现逻辑：解析修改/etc/ssh/sshd_config文件 ClientAliveInterval，其中ClientAliveCountMax设置为0

### 显示当前配置信息
show ssh config 
读取配置文件，格式化显示数据

### 显示用户登陆日志信息
show ssh user log xxx
实现逻辑：封装sudo cat /var/log/auth.log | grep "ssh" | grep "from" | tail -n xxx
读取日志信息并显示

### 显示当前在线用户信息
who命令


/usr/local/lib/python3.7/dist-packages/config/