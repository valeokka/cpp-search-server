// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:
#include <iostream>
#include <string>
using namespace std;

//Проверяем есть ли в числе 3
bool CheckForThree(int i) {
	string k = to_string(i);
	for (const char& number : k) {
		if (number == '3') {
			return true;
		}
	}
	return false;
}

int main() {

	int counter = 0;
	for (int i = 1000; i != 0; --i) {
		bool checker = CheckForThree(i);
		if (checker == true)
			++counter;
	}
	cout << counter; // В 271 числе
}

// Закомитьте изменения и отправьте их в свой репозиторий.
