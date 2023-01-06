# FdbOrch类解析

## 构造函数

```
gFdbOrch = new FdbOrch(m_applDb, app_fdb_tables, stateDbFdb, gPortsOrch);
FdbOrch(DBConnector* applDbConnector, vector<table_name_with_pri_t> appFdbTables, TableConnector stateDbFdbConnector, PortsOrch *port);
```

- 将appFdbTables加入Orch中（consumerstatetable）
- 创建appFdbTables对应的表，加入m_apptable
- this指针attach到port对象的observers中，port对象状态发生变化时会调用FdbOrch的update接口通知（观察者模式）
- 订阅频道：FLUSHFDBREQUEST（APP_DB）、FDB_NOTIFICATIONS（ASIC_DB），由dotask处理

## FdbOrch::bake

- consumer中的所有消息重写到同步队列中，等待刷新。（FDB_TABLE FDB_TABLE VXLAN_FDB_TABLE fdbNotifier flushNotifier）
- STATE_FDB_TABLE_NAME表信息也重入同步队列中，等待刷新




