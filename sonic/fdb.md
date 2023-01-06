# SONiC FDB分析

## 概述

* fdbsyncd：运行在swss容器中，通过订阅redis消息维护本地map数据，同时订阅netlink消息，处理链路和fdb变化消息
* fdborch：运行在swss容器中，fdb运行消息分发处理
* syncd：订阅APPDB处理fdb下发消息，注册fdb_event，处理芯片上报的fdb事件

## fdbsyncd分析

### netlink事件

继承类NetMsg，订阅netlink事件并加入组播组，通过NetDispatcher注册事件处理回调，由FdbSync::onMsg处理事件。订阅的事件有（[netlink](https://man7.org/linux/man-pages/man7/rtnetlink.7.html)）：

* RTM_NEWLINK：Create, remove, or get information about a specific
  network interface
* RTM_NEWNEIGH：Add, remove, or receive information about a neighbor table

其中，LINK事件只处理和vxlan相关事件，和mac表无关，细节暂不分析。

NEIGH事件由FdbSync::onMsgNbr处理，核心处理流程为:

1. 检查是否为AF_BRIDGE类型，非AF_BRIDGE类型无需处理
2. 对mac信息，刷新本地mac表（macRefreshStateDB）。如果需要新增本地mac表，可修改本接口实现
3. 对于vni消息，咱不关心处理逻辑。

### FDB 状态信息处理

订阅DB_STATE数据库表FDB_TABLE键空间，处理FDB状态信息，处理接口为FdbSync::processStateFdb，解析fdb消息，调用FdbSync::updateLocalMac，更新本地缓存m_fdb_mac，然后调用exec执行“bridge fdb add/del”命令更新内核fdb表。

FDB_TABLE表数据来源有：

* fdborch：APP_DB中添加fdb_entry时，会写入FDB_TABLE
* fdborch：在bake中（warmstart），重新读取FDB_TABLE数据处理
* fdborch：订阅频道NOTIFICATIONS。当ASIC学习mac产生新的fdb后，会通知syncd，syncd将fdb信息发布到本频道，由FdbOrch::doTask(swss::NotificationConsumer& consumer)处理

## FDB 事件处理

在sonic中，当有ASIC硬件学习mac表时，ASIC会将学习的mac表通过事件上报给sonic，sonic在syncd中启动线程NotificationProcessor处理事件，并发布到频道REDIS_TABLE_NOTIFICATIONS(RedisNotificationProducer), orchagent订阅该频道并处理事件，写入state db。

其中，RedisNotificationProducer线程处理asic上报的信息，NotificationHandler通过sai接口注册回调，将asic上报消息写入队列，syncd中的初始化流程摘录代码如下：

```c
syncd_main(argc, argv);   // main.cpp   syncd守护进程初始化流程
    // 创建syncd对象
 -- auto syncd = std::make_shared<Syncd>(vendorSai, commandLineOptions, isWarmStart);  
    // 通过send接口发布消息到频道： ASIC_DB： "NOTIFICATIONS"
   -- m_notifications = std::make_shared<RedisNotificationProducer>(m_contextConfig->m_dbAsic); 
   // ASIC DB视图，操作类
   -- m_client = std::make_shared<RedisClient>(m_dbAsic); 
   // 创建线程，处理sdk上报消息（sdk将消息写入m_notificationQueue），其中， fdb消息的处理回调函数为：
   // ntf_process_function线程主循环  -> Syncd::syncProcessNotification(bind对象) ->
   // m_processor->syncProcessNotification  ->
   //  NotificationProcessor::handle_fdb_event  -> NotificationProcessor::process_on_fdb_event ->
   //  sendNotification(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT, s)
   //  sendNotification会写入频道NOTIFICATIONS，有swss中的fdborch订阅处理
   -- m_processor = std::make_shared<NotificationProcessor>(m_notifications, m_client, std::bind(&Syncd::syncProcessNotification, this, _1));

    // NotificationHandler初始化
    m_handler = std::make_shared<NotificationHandler>(m_processor); // 写消息到m_processor处理队列
    // 设置回调处理，其中NotificationHandler::onFdbEvent写消息到队列
    m_sn.onFdbEvent = std::bind(&NotificationHandler::onFdbEvent, m_handler.get(), _1, _2);
    m_handler->setSwitchNotifications(m_sn.getSwitchNotifications());  // 设置回调
```

在swss初始化时，通过set_switch_attribute初始化switch，此时，设置fdb等事件回调接口到sdk中，处理流程为:

```c
    // attr中设置了SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY
    orchagent初始化： sai_switch_api->set_switch_attribute(gSwitchId, &attr);

    // 在syncd中处理订阅处理订阅的RedisSelectableChannel消息时， 在sai_status_t Syncd::processQuadEvent接口处理
    // 1. 从m_handler中更新回调，也就是说，当前在orchagent中设置额回调无效，在syncd中重新更新为m_handler中设置的回调
    // 2. processEntry处理消息，下发asic
    // 3. 调用VendorSai的接口处理下发
    m_handler->updateNotificationsPointers(attr_count, attr_list);
    status = processEntry(metaKey, api, attr_count, attr_list);
       -- m_vendorSai->create(metaKey, SAI_NULL_OBJECT_ID, attr_count, attr_list);
        -- sai_metadata_sai_switch_api实际处理，该api在VendorSai初始化时注册，由SDK提供
          -- centec的处理接口：ctc_sai_switch_create_switch 
                   -- p_switch_master->fdb_event_cb = (sai_fdb_event_notification_fn)attr_val->ptr // 设置回调
                   -- sdk在初始化时会启动线程（saiFdbFlushThread），处理asic上送的消息，并通过p_switch_master->fdb_event_cb上送
```

## FdbOrch分析

FdbOrch运行在swss容器orchagent进程中，构建并初始化FdbOrch对象，加入select中，有主进程负责处理对应事件。

### 构造函数

```c
FdbOrch::FdbOrch(DBConnector* applDbConnector, vector<table_name_with_pri_t> appFdbTables, TableConnector stateDbFdbConnector, PortsOrch *port)
```

* 将appFdbTables加入Orch中（consumerstatetable）, 有新的fdb加入时，应用模块会写入该表，orch订阅后通过doTask处理，更新数据
* this指针attach到port对象的observers中，port对象状态发生变化时会调用FdbOrch的update接口通知（观察者模式）
* 订阅频道：FLUSHFDBREQUEST（APP_DB）、FDB_NOTIFICATIONS（ASIC_DB），由dotask处理（装饰器模式）

### FdbOrch::bake

- consumer中的所有消息重写到同步队列中，等待刷新。（FDB_TABLE FDB_TABLE VXLAN_FDB_TABLE fdbNotifier flushNotifier）
- STATE_FDB_TABLE_NAME表信息也重入同步队列中，等待刷新

# Dot1q实现分析

需要实现的功能点：

* 接口mac学习方式切换。接口mac学习方式默认为SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW（硬件学习）。在支持Dot1x时，需要修改模式为SAI_BRIDGE_PORT_FDB_LEARNING_MODE_CPU_TRAP(未知源mac报文trap到cpu)。
  提供接口，用于更新port mac学习模式，可参考MclagLink::setPortMacLearnMode实现。需要注意的是：
  1. 在dot1x使能时，需要检查mac学习模式是否冲突，比如接口已经使能了mclag，则不能再使能dot1x，注意互斥
  2. 在dot1x使能和去使能时，需要刷新fdb表
* 表项添加删除。提供接口用于添加或者删除fdb表， 实现同样参考maclag
* 同步所有数据给hostap，当hostap重启时，同步所有信息（APP_DB）给hostap


需要处理的事件：

端口 up/down  vlan成员加入/移除


// dot1x认证fdb不应该被老化， 设置为静态vlan

// 端口up/down时通知hostap， 并主动flush表项。host在下线时会再次删除表项，此时，重复处理

// vlan从成员口中删除时，通知hostap，同时查找所有fdb表项，删除
