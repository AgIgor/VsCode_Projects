Import('env')
import os
import shutil

def build_littlefs(source, target, env):
    print("=" * 50)
    print("LittleFS: Preparando arquivos...")
    print("=" * 50)
    
    data_dir = os.path.join(env['PROJECT_DIR'], 'data')
    if os.path.exists(data_dir):
        print(f"Pasta data encontrada: {data_dir}")
        for root, dirs, files in os.walk(data_dir):
            for file in files:
                print(f"  - {os.path.join(root, file)}")
    else:
        print("⚠️  Pasta 'data' não encontrada!")

env.AddPreAction("$BUILD_DIR/littlefs.bin", build_littlefs)
