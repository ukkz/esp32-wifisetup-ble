#include "FS.h"

class Store {
  private:
    fs::FS& fs;
    const char* rofile;
    const char* rwfile;
    bool exist(const String filename, const String key) {
      bool result = false;
      File f = fs.open(filename);
      while (true) {
        // 1行ずつ読み込む
        String line = f.readStringUntil('\n');
        // 何もなくなれば離脱
        if (!line.length()) break;
        // キーを探す
        int i = line.indexOf(key+":");
        if (i == 0) result = true;
      }
      f.close();
      return result;
    }
    String read(const String filename, const String key) {
      String val = String("");
      File f = fs.open(filename);
      while (true) {
        // 1行ずつ読み込む
        String line = f.readStringUntil('\n');
        // 何もなくなれば離脱
        if (!line.length()) break;
        // キーを探す
        int i = line.indexOf(key+":");
        if (i == 0) {
          // value先頭位置
          int s = line.indexOf(":") + 1;
          // 行末まで切り出す
          val = line.substring(s);
          break;
        }
      }
      f.close();
      return val;
    }
    
  public:
    Store(fs::FS& fs_obj) : fs(fs_obj) {}
    void begin(const char* read_write_filename, const char* read_only_filename = "") {
      rwfile = read_write_filename;
      rofile = read_only_filename;
      // ReadWriteファイルが存在しなければあらかじめ作成しておく
      if (!fs.exists(rwfile)) {
        log_i("File \"%s\" not found - created");
        File f = fs.open(rwfile, "w");
        f.write('\n');
        f.close();
      }
    }
    String get(const String key) {
      // ReadOnlyを優先的に返す（同じキーが書き込まれたとき対策）
      if (!String(rofile).equals("")) {
        String ro = read(rofile, key);
        if (!ro.equals("")) return ro;
      }
      // ReadOnlyファイルが指定されていないかキーが存在しなければReadWriteファイルから取得
      String rw = read(rwfile, key);
      if (!rw.equals("")) return rw;
      return "";
    }
    void getCharArray(const String key, char* charArray) {
      // c_str()ではなくこちらを推奨（ポインタが消えてる可能性あるため）
      String val = get(key);
      val.toCharArray(charArray, val.length()+1);
    }
    bool set(const String key, const String value) {
      // ReadWriteファイルを更新対象行を除いて全部読む
      String buf = String("");
      File f = fs.open(rwfile);
      while (true) {
        // 1行ずつ読む
        String line = f.readStringUntil('\n');
        // 何もなくなれば離脱
        if (!line.length()) break;
        // 対象のキーが行内に存在するか
        if (line.indexOf(key) == -1) {
          // 対象のキーでなければ退避バッファに追加
          buf = buf + line + '\n';
        } else {
          // 対象のキーがあればその行は無視して新しいvalueを持った行を退避バッファに追加
          buf = buf + key + ":" + value + '\n';
        }
      }
      f.close();
      // 対象のキーが無かった場合は新規に追加として末尾に追記する
      if (buf.indexOf(key+":") == -1) buf = buf + key + ":" + value + '\n';
      // ReadWriteファイルを削除
      bool d = fs.remove(rwfile);
      if (!d) {
        log_e("Couldn't delete: %s", rwfile);
        return false;
      }
      // ReadWriteファイルを再作成して退避していたバッファを書き込む
      File rw = fs.open(rwfile, "w");
      if (!rw) {
        log_e("Couldn't open: %s", rwfile);
        return false;
      }
      rw.print(buf);
      rw.close();
      return true;
    }
    bool setIfFalse(const String key, const String value) {
      // false判定（数値0か空文字列）であるときのみ値をセットする
      String target_value = read(rwfile, key);
      if (target_value.equals("") || target_value.toInt() == 0 || target_value.toFloat() == 0.0) {
        return set(key, value);
      } else {
        return false;
      }
    }
};
