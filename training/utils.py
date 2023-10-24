import os
import tarfile

def save_model(source_dir, knowledge_file):
    with tarfile.open(knowledge_file, "w:gz", dereference=True) as tar:
        for f in os.listdir(source_dir):
            tar.add(os.path.join(source_dir, f), arcname=f)
