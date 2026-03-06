import re

NUM_BINS = 8192 # Фіксований розмір вектора

def fnv1a_32(string):
    """Швидкий і стабільний хеш алгоритм FNV-1a"""
    hash_val = 0x811c9dc5
    for char in string.encode('utf-8'):
        hash_val ^= char
        hash_val = (hash_val * 0x01000193) & 0xFFFFFFFF
    return hash_val

def extract_words(text):
    """Токенізація: тільки букви та цифри, нижній регістр"""
    return [w.lower() for w in re.findall(r'[a-zA-Z0-9]+', text)]

def vectorize_text_hashed(text):
    """Перетворює текст на бінарний масив розміру NUM_BINS (Уніграми + Біграми)"""
    vector = [0.0] * NUM_BINS
    words = extract_words(text)
    
    # Уніграми (окремі слова)
    for w in words:
        idx = fnv1a_32(w) % NUM_BINS
        vector[idx] = 1.0
        
    # Біграми (пари слів)
    for i in range(len(words) - 1):
        bigram = words[i] + " " + words[i+1]
        idx = fnv1a_32(bigram) % NUM_BINS
        vector[idx] = 1.0
        
    return vector