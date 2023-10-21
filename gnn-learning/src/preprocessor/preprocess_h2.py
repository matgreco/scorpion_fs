import subprocess

def run_h2_preprocessor_on_file(path_to_file: str, time_limit,  **kwargs):
    h2_path = ['scorpion/builds/release/bin/preprocess-h2']
    
    if kwargs is None:
        kwargs = {'preexec_fn': None}

    with open(path_to_file, "r") as f:
        try:
            proc = subprocess.Popen(h2_path, stdin=f, stderr=subprocess.PIPE, **kwargs)
            res = proc.communicate(timeout=time_limit)
        except subprocess.TimeoutExpired:
            proc.kill()
            res = proc.communicate()
            print("H2 is taking too long, let's kill it")