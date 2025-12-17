import serial
import mysql.connector
from mysql.connector import Error

SERIAL_PORT = "COM5"
BAUD_RATE = 115200

DB_CONFIG = {
    "host": "YOUR_HOST",
    "user": "YOUR_USERNAME",
    "password": "YOUR_PASSWORD",
    "database": "YOUR_DATABASE"
}

# Valid labels from Arduino
VALID_LABELS = {
    "Gmail",
    "Microsoft",
    "Linked in",
    "Twitter (X)",
    "Reddit",
}

current_user = None
current_label = None


# ---------------- DATABASE FUNCTIONS ---------------- #

def save_entry(user_id, label, password):
    """Insert or update password for a given user + label."""
    try:
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()

        sql = """
        INSERT INTO password_logs (user_id, account_label, password_value)
        VALUES (%s, %s, %s)
        ON DUPLICATE KEY UPDATE
            password_value = VALUES(password_value)
        """

        cursor.execute(sql, (user_id, label, password))
        conn.commit()

        print(f"[DB] Saved → User {user_id} - {label} - {password}")

    except Error as e:
        print("[MySQL Error]:", e)

    finally:
        if conn.is_connected():
            cursor.close()
            conn.close()


def view_all():
    """Display all stored passwords ALWAYS grouped as User 1, User 2, User 3."""
    try:
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()

        cursor.execute("""
            SELECT user_id, account_label, password_value
            FROM password_logs
            ORDER BY user_id, account_label
        """)

        rows = cursor.fetchall()

        # Group rows by user
        grouped = {1: [], 2: [], 3: []}
        for user_id, label, password in rows:
            if user_id in grouped:
                grouped[user_id].append((label, password))

        # Print ALWAYS in order 1 → 2 → 3
        for user_id in [1, 2, 3]:
            print(f"User {user_id}:")
            if grouped[user_id]:
                for label, password in grouped[user_id]:
                    print(f" - {label}: {password}")
            else:
                print(" (no accounts)")
            print("")

    except Error as e:
        print("[MySQL Error]:", e)

    finally:
        try:
            if conn.is_connected():
                cursor.close()
                conn.close()
        except:
            pass


# ---------------- SERIAL READER ---------------- #

def main():
    global current_user, current_label

    print("Listening on serial...")
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

    while True:
        raw = ser.readline().decode("utf-8", errors="ignore").strip()
        if not raw:
            continue

        print("Received:", raw)

        # USER ID SIGNAL
        if raw.startswith("USR:"):
            try:
                user_value = int(raw.replace("USR:", "").strip())
                if 1 <= user_value <= 3:
                    current_user = user_value
                    print(f"[INFO] Logged in as User {current_user}")
                else:
                    print("[WARN] Invalid user id!")
            except:
                print("[WARN] Could not parse user id!")

        # LABEL SIGNAL
        elif raw.startswith("LBL:"):
            label = raw.replace("LBL:", "").strip()

            if label in VALID_LABELS:
                current_label = label
                print(f"[INFO] Selected Label: {current_label}")
            else:
                print(f"[WARN] Invalid label: {label}")
                current_label = None

        # PASSWORD SIGNAL
        elif raw.startswith("PW:"):
            password = raw.replace("PW:", "").strip()

            if current_user is None:
                print("[WARN] Password received but no user logged in!")
                continue

            if current_label is None:
                print("[WARN] Password received but no valid label selected!")
                continue

            save_entry(current_user, current_label, password)

            # reset label after save
            current_label = None


if __name__ == "__main__":
    main()
