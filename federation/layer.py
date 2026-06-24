import hashlib

class Federation:
    def encode_room(self, memory):
        profile_str = str(memory.get("room_profile", {}))
        return int(hashlib.md5(profile_str.encode()).hexdigest()[:8], 16)

    def share(self, room_a, room_b):
        if not room_a or not room_b:
            return 0.0
        diff = abs(room_a - room_b)
        max_val = max(room_a, room_b)
        similarity = 1.0 - (diff / max_val) if max_val > 0 else 0.0
        return round(similarity, 3)