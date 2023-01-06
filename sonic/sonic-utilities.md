# 对命令行框架sonic-utilities的开发介绍

sonic-utilities是一个python wheel package，包含了所有sonic命令行python代码。

## 1. 编译框架
sonic-utilities作为命令行组件，提供sonic中config、show、install等命令。其代码在路径sonic-buildimage/src/sonic-utilities下， 以子模块的方式集成到sonic-buildimage框架中。
共有连个编译目标：
- sonic_utilities-1.2-py3-none-any.whl命令包，python setup.py工具打包了所有命令，使用pip安装
- sonic-utilities-data_1.0-1_all.deb   配置数据包，主要打包了bash自动补齐配置，通过dkpg_buildpackage打包。

### 1.1 编译流程

1. 配置编译环境
   
    `make init && make configure PLATFORM=xxx
    
    其中，PLATFORM可选择自己当前编译的平台
2. 编译打包wheel包， wheel包在sonic中的编译流程在须后章节介绍
   `make NOJESSIE=1 NOSTRETCH=1 KEEP_SLAVE_ON=yes target/python-wheels/sonic_utilities-1.2-py3-none-any.whl
   其中，参数KEEP_SLAVE_ON表示编译完成之后，容器不退出
3. 在上述步骤完整之后， shell会停留在容器中，在容器进入sonic-utilities代码路径
4. 然后可以手动执行命令打包和测试wheel包
   ```
   python3 setup.py bdist_wheel   # build
   python3 setup.py test          # 测试
   ```
### 1.2 sonic_utilities-1.2-py3-none-any.whl

sonic-utilities的编译target定义在文件rule/sonic-utilities.mk中，通过SONIC_PYTHON_WHEELS目标编译。摘抄并说明如下：
```
# 编译目标及路径、依赖等定义
SONIC_UTILITIES_PY3 = sonic_utilities-1.2-py3-none-any.whl
$(SONIC_UTILITIES_PY3)_TEST = n
$(SONIC_UTILITIES_PY3)_SRC_PATH = $(SRC_PATH)/sonic-utilities
$(SONIC_UTILITIES_PY3)_PYTHON_VERSION = 3
$(SONIC_UTILITIES_PY3)_DEPENDS += $(SONIC_PY_COMMON_PY3) \
                                  $(SWSSSDK_PY3) \
                                  $(SONIC_CONFIG_ENGINE_PY3) \
                                  $(SONIC_PLATFORM_COMMON_PY3) \
                                  $(SONIC_YANG_MGMT_PY3) \
                                  $(SONIC_YANG_MODELS_PY3)
$(SONIC_UTILITIES_PY3)_DEBS_DEPENDS = $(LIBYANG) \
                                      $(LIBYANG_CPP) \
                                      $(LIBYANG_PY3) \
                                      $(LIBSWSSCOMMON) \
                                      $(PYTHON3_SWSSCOMMON)
# 加入到SONIC_PYTHON_WHEELS 中，在SONIC_PYTHON_WHEELS 编译时会遍历所有的WHEEL并执行后续流程
SONIC_PYTHON_WHEELS += $(SONIC_UTILITIES_PY3)

```

实际编译流程定义在slave.mk中。解析目标及依赖关系，进入代码路径并执行setup.py bdist_wheel生成whl包：
此处，在sonic中使用python的setup-tools生成whl包并测试。setup-tools的介绍见下文。然后使用pytest执行自动测试用例，pytest的介绍也见下文。
 
**如果不想执行setup test，可以设置参数$(SONIC_UTILITIES_PY3)_TEST = n**
```
# Projects that generate python wheels
# Add new package for build:
#     SOME_NEW_WHL = some_new_whl.whl
#     $(SOME_NEW_WHL)_SRC_PATH = $(SRC_PATH)/project_name
#     $(SOME_NEW_WHL)_PYTHON_VERSION = 2 (or 3)
#     $(SOME_NEW_WHL)_DEPENDS = $(SOME_OTHER_WHL1) $(SOME_OTHER_WHL2) ...
#     SONIC_PYTHON_WHEELS += $(SOME_NEW_WHL)
$(addprefix $(PYTHON_WHEELS_PATH)/, $(SONIC_PYTHON_WHEELS)) : $(PYTHON_WHEELS_PATH)/% : .platform $$(addsuffix -install,$$(addprefix $(PYTHON_WHEELS_PATH)/,$$($$*_DEPENDS))) \
                        $(call dpkg_depend,$(PYTHON_WHEELS_PATH)/%.dep) \
                        $$(addsuffix -install,$$(addprefix $(DEBS_PATH)/,$$($$*_DEBS_DEPENDS)))
        $(HEADER)

        # Load the target deb from DPKG cache
        $(call LOAD_CACHE,$*,$@)

        # Skip building the target if it is already loaded from cache
        if [ -z '$($*_CACHE_LOADED)' ] ; then

                pushd $($*_SRC_PATH) $(LOG_SIMPLE)
                # apply series of patches if exist
                if [ -f ../$(notdir $($*_SRC_PATH)).patch/series ]; then QUILT_PATCHES=../$(notdir $($*_SRC_PATH)).patch quilt push -a; fi
                # Use pip instead of later setup.py to install dependencies into user home, but uninstall self
                pip$($*_PYTHON_VERSION) install . && pip$($*_PYTHON_VERSION) uninstall --yes `python setup.py --name`
                info $($($*_TEST))
                if [ ! "$($*_TEST)" = "n" ]; then python$($*_PYTHON_VERSION) setup.py test $(LOG); fi
                python$($*_PYTHON_VERSION) setup.py bdist_wheel $(LOG)
                # clean up
                if [ -f ../$(notdir $($*_SRC_PATH)).patch/series ]; then quilt pop -a -f; [ -d .pc ] && rm -rf .pc; fi
                popd $(LOG_SIMPLE)
                mv -f $($*_SRC_PATH)/dist/$* $(PYTHON_WHEELS_PATH) $(LOG)

                # Save the target deb into DPKG cache
                $(call SAVE_CACHE,$*,$@)
        fi

        # Uninstall unneeded build dependency
        $(call UNINSTALL_DEBS,$($*_UNINSTALLS))

        $(FOOTER)
```

### 1.3 sonic-utilities-data_1.0-1_all.deb
sonic-utilities-data_1.0-1_all.deb的编译文件定义在rule/sonic-utilities-data.mk中：
```
SONIC_UTILITIES_DATA = sonic-utilities-data_1.0-1_all.deb
$(SONIC_UTILITIES_DATA)_SRC_PATH = $(SRC_PATH)/sonic-utilities/sonic-utilities-data
SONIC_DPKG_DEBS += $(SONIC_UTILITIES_DATA)
```

其中SONIC_DPKG_DEBS的编译方式定义在slave.mk中, 使用 dpkg-buildpackage进行编译：
```
# Build project with dpkg-buildpackage
# Add new package for build:
#     SOME_NEW_DEB = some_new_deb.deb
#     $(SOME_NEW_DEB)_SRC_PATH = $(SRC_PATH)/project_name
#     $(SOME_NEW_DEB)_DEPENDS = $(SOME_OTHER_DEB1) $(SOME_OTHER_DEB2) ...
#     SONIC_DPKG_DEBS += $(SOME_NEW_DEB)
$(addprefix $(DEBS_PATH)/, $(SONIC_DPKG_DEBS)) : $(DEBS_PATH)/% : .platform $$(addsuffix -install,$$(addprefix $(DEBS_PATH)/,$$($$*_DEPENDS))) \
                        $$(addprefix $(DEBS_PATH)/,$$($$*_AFTER)) \
                        $(call dpkg_depend,$(DEBS_PATH)/%.dep )
        $(HEADER)

        # Load the target deb from DPKG cache
        $(call LOAD_CACHE,$*,$@)

        # Skip building the target if it is already loaded from cache
        if [ -z '$($*_CACHE_LOADED)' ] ; then

                # Remove old build logs if they exist
                rm -f $($*_SRC_PATH)/debian/*.debhelper.log
                # Apply series of patches if exist
                if [ -f $($*_SRC_PATH).patch/series ]; then pushd $($*_SRC_PATH) && QUILT_PATCHES=../$(notdir $($*_SRC_PATH)).patch quilt push -a; popd; fi
                # Build project
                pushd $($*_SRC_PATH) $(LOG_SIMPLE)
                if [ -f ./autogen.sh ]; then ./autogen.sh $(LOG); fi
                $(SETUP_OVERLAYFS_FOR_DPKG_ADMINDIR)
                $(if $($*_DPKG_TARGET),
                        ${$*_BUILD_ENV} DEB_BUILD_OPTIONS="${DEB_BUILD_OPTIONS_GENERIC} ${$*_DEB_BUILD_OPTIONS}" dpkg-buildpackage -rfakeroot -b -us -uc -j$(SONIC_CONFIG_MAKE_JOBS) --as-root -T$($*_DPKG_TARGET) --admindir $$mergedir $(LOG),
                        ${$*_BUILD_ENV} DEB_BUILD_OPTIONS="${DEB_BUILD_OPTIONS_GENERIC} ${$*_DEB_BUILD_OPTIONS}" dpkg-buildpackage -rfakeroot -b -us -uc -j$(SONIC_CONFIG_MAKE_JOBS) --admindir $$mergedir $(LOG)
                )
                popd $(LOG_SIMPLE)
                # Clean up
                if [ -f $($*_SRC_PATH).patch/series ]; then pushd $($*_SRC_PATH) && quilt pop -a -f; [ -d .pc ] && rm -rf .pc; popd; fi
                # Take built package(s)
                mv -f $(addprefix $($*_SRC_PATH)/../, $* $($*_DERIVED_DEBS) $($*_EXTRA_DEBS)) $(DEBS_PATH) $(LOG)

                # Save the target deb into DPKG cache
                $(call SAVE_CACHE,$*,$@)
        fi

        # Uninstall unneeded build dependency
        $(call UNINSTALL_DEBS,$($*_UNINSTALLS))

        $(FOOTER)
```

## 2. 代码框架

sonic-utilities代码以 sub module的模块集成到sonic-buildimage中，其代码仓库为https://github.com/Azure/sonic-utilities，代码路径为src/sonic-utilities


sonic-utilities的代码主要有三部分组成:

- sonic命令行所有python代码，通过python setup.py  bdist_wheel编译，对应的packet包有由setup.cfg描述，改文件描述了需要打包的代码路径、安装依赖、脚本、测试依赖等信息，setup模块根据该文件的描述，打包生成sonic_utilities-1.2-py3-none-any.whl
- sonic命令行的data包，路径为sonic-utilities-data，在编译框架中定义dpkg_build包，该模块描述了sonic命令行的配置数据，主要是bash自动补齐配置
- test文件及test函数，描述sonic命令行自动化测试用例，在编译后可执行setup.py test, 对命令行框架代码做自动化测试。如之前描述，该部分基于pytest框架实现，在执行setup.py test时，会当前路径下的所有符合pytest定义的函数，每个函数为一个测试用例，实现自动化测试。

### 2.1 show vlan brief命令举例

sonic命令基于click模块实现，Click 是一个利用很少的代码以可组合的方式创造优雅命令行工具接口的 Python 库。关于click的详情，请参见后文描述或官方文档。

show vlan brief命令用于显示当前vlan信息，命令执行结果如下所示：
```
admin@sonic:~$ show vlan brief
+-----------+-----------------+-------------+----------------+-----------------------+-------------+
|   VLAN ID | IP Address      | Ports       | Port Tagging   | DHCP Helper Address   | Proxy ARP   |
+===========+=================+=============+================+=======================+=============+
|       100 | 110.1.18.218/16 | Ethernet20  | tagged         |                       | disabled    |
|           |                 | Ethernet24  | tagged         |                       |             |
|           |                 | Ethernet28  | untagged       |                       |             |
|           |                 | Ethernet32  | untagged       |                       |             |
|           |                 | Ethernet100 | tagged         |                       |             |
+-----------+-----------------+-------------+----------------+-----------------------+-------------+

```

show vlan的代码见sonic-utilities/show/vlan.py。所有的命令行python文件，均实现两个功能：
- 通过click框架注册命令行
- 实现命令行执行回调
  
在这里show vlan brief命令的实现函数摘抄如下，其功能为：

1. 调用DB工具类连接redis数据库（CFG_DB/APP_DB/STATE_DB等）
2. 读取数据库，组织输出

```
import click
from natsort import natsorted
from tabulate import tabulate

import utilities_common.cli as clicommon

@click.group(cls=clicommon.AliasedGroup)
def vlan():
    """Show VLAN information"""
    pass

@vlan.command()
@click.option('--verbose', is_flag=True, help="Enable verbose output")
@clicommon.pass_db
def brief(db, verbose):
    """Show all bridge information"""
    header = ['VLAN ID', 'IP Address', 'Ports', 'Port Tagging', 'DHCP Helper Address', 'Proxy ARP']
    body = []

    # Fetching data from config db for VLAN, VLAN_INTERFACE and VLAN_MEMBER
    vlan_dhcp_helper_data = db.cfgdb.get_table('VLAN')
    vlan_ip_data = db.cfgdb.get_table('VLAN_INTERFACE')
    vlan_ports_data = db.cfgdb.get_table('VLAN_MEMBER')

    # Defining dictionaries for DHCP Helper address, Interface Gateway IP,
    # VLAN ports and port tagging
    vlan_dhcp_helper_dict = {}
    vlan_ip_dict = {}
    vlan_ports_dict = {}
    vlan_tagging_dict = {}
    vlan_proxy_arp_dict = {}

    # Parsing DHCP Helpers info
    for key in natsorted(list(vlan_dhcp_helper_data.keys())):
        try:
            if vlan_dhcp_helper_data[key]['dhcp_servers']:
                vlan_dhcp_helper_dict[key.strip('Vlan')] = vlan_dhcp_helper_data[key]['dhcp_servers']
        except KeyError:
            vlan_dhcp_helper_dict[key.strip('Vlan')] = " "

    # Parsing VLAN Gateway info
    for key in vlan_ip_data:
        if clicommon.is_ip_prefix_in_key(key):
            interface_key = key[0].strip("Vlan")
            interface_value = key[1]

            if interface_key in vlan_ip_dict:
                vlan_ip_dict[interface_key].append(interface_value)
            else:
                vlan_ip_dict[interface_key] = [interface_value]
        else:
            interface_key = key.strip("Vlan")
            if 'proxy_arp' in vlan_ip_data[key]:
                proxy_arp_status = vlan_ip_data[key]['proxy_arp']
            else:
                proxy_arp_status = "disabled"

            vlan_proxy_arp_dict[interface_key] = proxy_arp_status



    iface_alias_converter = clicommon.InterfaceAliasConverter(db)

    # Parsing VLAN Ports info
    for key in natsorted(list(vlan_ports_data.keys())):
        ports_key = key[0].strip("Vlan")
        ports_value = key[1]
        ports_tagging = vlan_ports_data[key]['tagging_mode']
        if ports_key in vlan_ports_dict:
            if clicommon.get_interface_naming_mode() == "alias":
                ports_value = iface_alias_converter.name_to_alias(ports_value)
            vlan_ports_dict[ports_key].append(ports_value)
        else:
            if clicommon.get_interface_naming_mode() == "alias":
                ports_value = iface_alias_converter.name_to_alias(ports_value)
            vlan_ports_dict[ports_key] = [ports_value]
        if ports_key in vlan_tagging_dict:
            vlan_tagging_dict[ports_key].append(ports_tagging)
        else:
            vlan_tagging_dict[ports_key] = [ports_tagging]

    # Printing the following dictionaries in tablular forms:
    # vlan_dhcp_helper_dict={}, vlan_ip_dict = {}, vlan_ports_dict = {}
    # vlan_tagging_dict = {}
    for key in natsorted(list(vlan_dhcp_helper_dict.keys())):
        if key not in vlan_ip_dict:
            ip_address = ""
        else:
            ip_address = ','.replace(',', '\n').join(vlan_ip_dict[key])
        if key not in vlan_ports_dict:
            vlan_ports = ""
        else:
            vlan_ports = ','.replace(',', '\n').join((vlan_ports_dict[key]))
        if key not in vlan_dhcp_helper_dict:
            dhcp_helpers = ""
        else:
            dhcp_helpers = ','.replace(',', '\n').join(vlan_dhcp_helper_dict[key])
        if key not in vlan_tagging_dict:
            vlan_tagging = ""
        else:
            vlan_tagging = ','.replace(',', '\n').join((vlan_tagging_dict[key]))
        vlan_proxy_arp = vlan_proxy_arp_dict.get(key, "disabled")
        body.append([key, ip_address, vlan_ports, vlan_tagging, dhcp_helpers, vlan_proxy_arp])
    click.echo(tabulate(body, header, tablefmt="grid"))
```

### 2.2.2 sonic-utilities-data举例

sonic-utilities-data为通过dpkg build打包的deb包，在sonic-utilities-data路径下描述了dpkg包文件：
```
city@sonic:~/bd-vs/sonic-buildimage/src/sonic-utilities/sonic-utilities-data$ ls -rtl
total 24
-rw-rw-r-- 1 city city  305 10月 19 15:08 README.md
-rw-rw-r-- 1 city city  242 10月 19 15:08 MAINTAINERS
-rw-rw-r-- 1 city city  712 10月 19 15:08 LICENSE
drwxrwxr-x 4 city city 4096 10月 22 14:39 debian
drwxrwxr-x 2 city city 4096 10月 22 14:39 bash_completion.d   # bash 补全配置
drwxrwxr-x 2 city city 4096 10月 22 14:40 templates           # 环境配置模板文件

```

### 2.2.3 vlan test举例
根据vlan_test.py中的用例逐个测试。在开始测试前， 从tests/mock_tables下加载测试基础数据。
```
class TestVlan(object):
    @classmethod
    def setup_class(cls):
        os.environ['UTILITIES_UNIT_TESTING'] = "1"
        # ensure that we are working with single asic config
        from .mock_tables import dbconnector
        from .mock_tables import mock_single_asic
        reload(mock_single_asic)   # 导入测试数据
        dbconnector.load_namespace_config()
        print("SETUP")

    def test_show_vlan(self):
        runner = CliRunner()
        result = runner.invoke(show.cli.commands["vlan"], [])
        print(result.exit_code)
        print(result.output)
        assert result.exit_code == 0  # 检测测试结果

```

### 2.2.4 DB操作封装接口介绍

src/sonic-py-swsssdk/src/swsssdk封装了对redis数据库的操作接口。

## 背景知识介绍
### python setup-tools介绍
[setup-tool介绍-知乎](https://docs.python.org/3/distutils/setupscript.html)

[Writing the Setup Script](https://docs.python.org/3/distutils/setupscript.html)

### pytest介绍
[pytest介绍1](https://blog.csdn.net/weixin_67553250/article/details/124674706)
[pytest介绍2](https://blog.csdn.net/weixin_44867493/article/details/123062129)
[pytest官网](https://docs.pytest.org/en/7.1.x/)

###  dpkg-buildpackage介绍

### bash tab补全
在sonic-utilities中，打包了sonic-utilities-data.deb, 其定义了命令行补全功能。
bash自动补全由complete实现，具体细节有兴趣再分析。

### click框架介绍
[官方文档](https://click-docs-zh-cn.readthedocs.io/zh/latest/)