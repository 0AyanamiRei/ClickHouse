import pytest
from helpers.cluster import ClickHouseCluster

cluster = ClickHouseCluster(__file__)
node = cluster.add_instance("node", main_configs=["configs/config.xml"], with_zookeeper=True)


@pytest.fixture(scope="module")
def start_cluster():
    try:
        cluster.start()
        yield cluster
    finally:
        cluster.shutdown()


def test_recovery_time_metric(start_cluster):
    node.query("CREATE DATABASE rdb ENGINE = Replicated('/test/test_recovery_time_metric', 'shard1', 'replica1');")
    node.query("CREATE TABLE rdb.t (x UInt32) ENGINE = MergeTree ORDER BY x;")
    node.exec_in_container(["bash", "-c", "rm /var/lib/clickhouse/metadata/rdb/t.sql"])
    node.restart_clickhouse()
    assert (
        node.query("SELECT any(recovery_time) FROM system.clusters;")
        != "0\n"
    )

