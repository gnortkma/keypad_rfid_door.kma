BÌNH THƯỜNG
    ↓
LCD: "Nhap mat khau"
    ↓
Nhập password bằng Keypad
    ↓
Nhấn #
    ├── Đúng → Servo mở cửa
    │          ↓
    │       chờ một khoảng thời gian
    │          ↓
    │       Servo đóng cửa
    │
    └── Sai → còn 2 lần → còn 1 lần
                       ↓
                 Sai lần thứ 3
                       ↓
       
                Servo khóa
                Khóa nhập 30s
                       ↓
                trở về đăng nhập


ADMIN
    ↓
Admin quét thẻ RFID
    ↓
LCD: "Nhap MK Admin"
    ↓
Nhập mật khẩu hiện tại
    ↓
    ├── Sai → thoát
    │
    └── Đúng
         ↓
    "Nhap MK moi"
         ↓
       nhấn #
         ↓
    đổi mật khẩu chung
