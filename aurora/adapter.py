class AuroraAdapter:
    def __init__(self):
        self.nodes = {}
        self.sync_state = None

    def register_node(self, node_id, metadata):
        self.nodes[node_id] = metadata

    def get_synchronized_frame(self):
        return {
            "nodes": self.nodes,
            "timestamp": "synced"
        }

    def health_report(self):
        return {"status": "ok", "nodes": len(self.nodes)}