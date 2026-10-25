"""Flask backend: runs the C Huffman program and serves the frontend.
Run:  python app.py   ->  open http://127.0.0.1:5000
"""
import json
import os
import re
import subprocess
import time
import uuid

from flask import Flask, Response, jsonify, request, send_file

BASE = os.path.dirname(os.path.abspath(__file__))
HUFF = os.path.join(BASE, "huff.exe" if os.name == "nt" else "huff")
WORK = os.path.join(BASE, "work")
os.makedirs(WORK, exist_ok=True)

app = Flask(__name__, static_folder="static", static_url_path="")
app.config["MAX_CONTENT_LENGTH"] = 5 * 1024 * 1024   # 5 MB upload limit


def build_c_program():
    """Compile huff.c if the binary is missing or older than the source."""
    src = os.path.join(BASE, "huff.c")
    if not os.path.exists(HUFF) or os.path.getmtime(HUFF) < os.path.getmtime(src):
        print("Compiling huff.c ...")
        subprocess.run(["gcc", src, "-o", HUFF, "-lm"], check=True)


def run_huff(mode, src, dst):
    """Run the C program with --json and return its parsed output."""
    r = subprocess.run([HUFF, mode, src, dst, "--json"],
                       capture_output=True, text=True, timeout=30)
    try:
        return json.loads(r.stdout)
    except ValueError:
        return {"error": "C program failed"}


def cleanup_old_files(max_age=3600):
    now = time.time()
    for name in os.listdir(WORK):
        path = os.path.join(WORK, name)
        if now - os.path.getmtime(path) > max_age:
            os.remove(path)


def remove(*paths):
    for p in paths:
        if os.path.exists(p):
            os.remove(p)


@app.get("/")
def index():
    return app.send_static_file("index.html")


@app.post("/api/compress")
def compress():
    cleanup_old_files()
    f = request.files.get("file")
    if not f or not f.filename.lower().endswith(".txt"):
        return jsonify(error="Please upload a .txt file"), 400

    uid = uuid.uuid4().hex                     # we never use the user's file name on disk
    src = os.path.join(WORK, uid + ".txt")
    dst = os.path.join(WORK, uid + ".huf")
    f.save(src)
    try:
        data = run_huff("c", src, dst)
    finally:
        remove(src)

    if "error" in data:
        remove(dst)
        return jsonify(data), 400
    data["id"] = uid
    data["name"] = os.path.basename(f.filename)
    return jsonify(data)


@app.get("/api/download/<uid>")
def download(uid):
    if not re.fullmatch(r"[0-9a-f]{32}", uid):
        return jsonify(error="Bad id"), 400
    path = os.path.join(WORK, uid + ".huf")
    if not os.path.exists(path):
        return jsonify(error="File expired, compress again"), 404
    return send_file(path, as_attachment=True, download_name="compressed.huf")


@app.post("/api/decompress")
def decompress():
    cleanup_old_files()
    f = request.files.get("file")
    if not f or not f.filename.lower().endswith(".huf"):
        return jsonify(error="Please upload a .huf file"), 400

    uid = uuid.uuid4().hex
    src = os.path.join(WORK, uid + ".huf")
    dst = os.path.join(WORK, uid + ".out")
    f.save(src)
    try:
        data = run_huff("d", src, dst)
        if "error" in data:
            return jsonify(data), 400
        with open(dst, "rb") as fh:
            content = fh.read()
    finally:
        remove(src, dst)

    return Response(content, mimetype="text/plain",
                    headers={"Content-Disposition": 'attachment; filename="restored.txt"'})


if __name__ == "__main__":
    build_c_program()
    app.run(debug=False)
