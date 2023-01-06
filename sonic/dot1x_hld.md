Dot1x high level design for SONiC

在SONiC中支持dot1x认证，通过hostap等开源组件实现dot1x认证，然后将认证后的FDB表项通告给SONiC，由SONiC负责下发交换芯片。

hostap和sonic间需要通过网络传递进程间消息，其中，有hostap主动发起的消息有：

1. 端口dot1x认证使能。参数：端口名
2. 端口dot1x认证去使能。参数：端口名
3. flush（清除）端口dot1x认证fdb表项。参数：端口名
4. 添加FDB表项。参数：端口名、vlan、mac
5. 删除FDB表项。参数：端口名、vlan、mac
6. 开始计费。参数：端口名、vlan、mac
7. 修改FDB表项（授权vlan变更等）。参数：端口名、vlan、mac
8. 获取当前所有的FDB表项。参数：端口。sonic返回当前所有表项。
9. 报文统计信息增量查询（hostap轮询查询，避免阻塞其他任务，考虑启用单独线程）。参数：端口名、vlan、mac

sonic上报事件：端口down、成员口vlan删除。hostap在收到这些事件后，更新表项信息

Account

在dot1x中，需要支持按用户流量计费。提供接口：

* 创建acl表：在接口使能dot1x时，创建acl表：dot1x-ingress-接口名、dot1x-egress-接口名
* 删除acl表：在接口去使能时，删除创建的acl表
* 开始计费：创建acl rule
* 停止计费：删除acl rule
* 查询统计信息：查询COUNTER数据库，并返回结果


acl table name格式： dot1x-ingress/egress-接口名
acl rule name格式： dot1x-ingress/egress-接口名:vlan-mac

### ACL-TABLE

dot1x用户在计费时，需要根据报文的mac地址和vlan信息做计费统计，当前AclTable类没有设置SAI_ACL_ACTION_TYPE_SET_SRC_MAC和SAI_ACL_ACTION_TYPE_SET_DST_MAC，不能给acl rule设置mac过滤，因此在acl_table_type_t增加类型ACL_TABLE_DOT1X。在函数AclTable::create增加：

```c
    if (type == ACL_TABLE_DOT1X)
    {
        // 增加按mac地址过滤报文
        attr.id = SAI_ACL_ACTION_TYPE_SET_SRC_MAC;
        attr.value.booldata = true;
        table_attrs.push_back(attr);

        attr.id = SAI_ACL_ACTION_TYPE_SET_DST_MAC;
        attr.value.booldata = true;
        table_attrs.push_back(attr);
    }
```

在接口使能dot1x时，创建acl表，并应用到接口上：

```c
        /*创建acl表， 计费统计按mac地址计费*/
        string acl_name = "dot1x:ingress:xxx";  // 按dot1x：+ 接口名方式命名
        FieldValueTuple desc_attr("policy_desc", "dot1x account ingress acl table for port xxx");
        acl_attrs.push_back(desc_attr);

        FieldValueTuple type_attr("type", "ACL_TABLE_DOT1X");
        acl_attrs.push_back(type_attr);

        FieldValueTuple port_attr("ports", xxx);
        acl_attrs.push_back(port_attr);

        FieldValueTuple port_attr("stage", "INGRESS");
        acl_attrs.push_back(port_attr);

        p_acl_table_tbl->set(acl_name, acl_attrs);

        // 创建出方向acl表，指定stage为EGRESS
```

### ACL-RULE

创建类AclRuleDot1x，参考类AclRuleMclag，实现接口validateAddMatch， 支持mac和vlan配置，并在基类AclRule::validateAddMatch增加对machevlan的配置支持：

```c
        else if (attr_name == MATCH_SRC_MAC || attr_name == MATCH_DST_MAC) 
        {
            MacAddress(attr_value).getMac(value.aclfield.data.mac);
            memset(value.aclfield.mask.mac, 0xFF, ETHER_ADDR_LEN);
        }
```

### 统计信息

在aclOrch中创建了定时器ACL_POLL_TIMER， 用于轮询获取acl rule统计信息，并写入了CountDB， 只需要订阅CountDB获取即可

### action

FieldValueTuple packet_attr("PACKET_ACTION", "FORWARD");
