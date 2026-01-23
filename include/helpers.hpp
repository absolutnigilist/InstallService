#pragma once
#include <string>
#include <string_view>

//---Удаление кавычек в начале и конце строки (если они есть) для string_view
template<class CharT>
constexpr std::basic_string_view<CharT> 
trimQuotesView(std::basic_string_view<CharT> sv) noexcept 
{
	//---Проверка длины строки
	if (sv.size() < 2) return sv;

	//---Проверка первых и последних символов
	const CharT first = sv.front();
	const CharT last = sv.back();

	//---Проверка на наличие кавычек
	const bool doubleQuoted = (first == CharT('"') && last == CharT('"'));
	const bool singleQuoted = (first == CharT('\'') && last == CharT('\''));

	//---Удаление кавычек
	if (doubleQuoted || singleQuoted) {
		sv.remove_prefix(1);
		sv.remove_suffix(1);
	}
	return sv;
}
//---Удаление кавычек в начале и конце строки (если они есть) для string
template<class CharT, class Traits, class Alloc>
std::basic_string<CharT, Traits, Alloc>
trimQuotes(std::basic_string<CharT, Traits, Alloc> s)
{
	//---Использование trimQuotesView для обработки строки
	const auto v = trimQuotesView<CharT>(std::basic_string_view<CharT>(s));
	
	//---Если строка не изменилась, возвращаем оригинал
	if (v.size() == s.size()) return s;
	
	//---Создание новой строки из обрезанного представления
	return std::basic_string<CharT, Traits, Alloc>(v);
}