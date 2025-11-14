"use client";

import { useState } from "react";
import Link from "next/link";

const outfits = [
  {
    id: 1,
    name: "Summer Breeze",
    category: "Casual",
    description: "Perfect untuk hangout santai di akhir pekan",
    items: ["White Linen Shirt", "Light Blue Jeans", "White Sneakers"],
    season: "Summer",
    occasion: "Casual Outing",
    color: "bg-gradient-to-br from-blue-50 to-cyan-50"
  },
  {
    id: 2,
    name: "Business Chic",
    category: "Formal",
    description: "Tampil profesional dan percaya diri di kantor",
    items: ["Navy Blazer", "White Dress Shirt", "Black Trousers", "Oxford Shoes"],
    season: "All Season",
    occasion: "Office/Meeting",
    color: "bg-gradient-to-br from-slate-50 to-gray-100"
  },
  {
    id: 3,
    name: "Street Style",
    category: "Urban",
    description: "Gaya urban yang edgy dan modern",
    items: ["Black Hoodie", "Cargo Pants", "High-top Sneakers", "Baseball Cap"],
    season: "Fall/Winter",
    occasion: "Streetwear",
    color: "bg-gradient-to-br from-zinc-50 to-stone-100"
  },
  {
    id: 4,
    name: "Elegant Evening",
    category: "Formal",
    description: "Sempurna untuk acara malam yang elegan",
    items: ["Black Suit", "Silk Tie", "Leather Dress Shoes", "Watch"],
    season: "All Season",
    occasion: "Evening Event",
    color: "bg-gradient-to-br from-purple-50 to-pink-50"
  },
  {
    id: 5,
    name: "Beach Vibes",
    category: "Casual",
    description: "Santai dan nyaman untuk liburan pantai",
    items: ["Floral Shirt", "Khaki Shorts", "Sandals", "Sunglasses"],
    season: "Summer",
    occasion: "Beach/Vacation",
    color: "bg-gradient-to-br from-orange-50 to-amber-50"
  },
  {
    id: 6,
    name: "Sporty Active",
    category: "Athletic",
    description: "Outfit olahraga yang stylish dan fungsional",
    items: ["Track Jacket", "Athletic Pants", "Running Shoes", "Sports Watch"],
    season: "All Season",
    occasion: "Sports/Gym",
    color: "bg-gradient-to-br from-green-50 to-emerald-50"
  }
];

export default function Home() {
  const [selectedCategory, setSelectedCategory] = useState("All");
  const categories = ["All", "Casual", "Formal", "Urban", "Athletic"];

  const filteredOutfits = selectedCategory === "All" 
    ? outfits 
    : outfits.filter(outfit => outfit.category === selectedCategory);

  return (
    <div className="min-h-screen bg-gradient-to-br from-gray-50 via-white to-gray-50">
      {/* Navigation */}
      <nav className="border-b border-gray-200 bg-white/80 backdrop-blur-md sticky top-0 z-50">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex justify-between items-center h-16">
            <div className="flex items-center">
              <h1 className="text-2xl font-bold bg-gradient-to-r from-gray-900 to-gray-600 bg-clip-text text-transparent">
                OutfitHub
              </h1>
            </div>
            <div className="hidden md:flex space-x-8">
              <a href="#home" className="text-gray-700 hover:text-gray-900 transition-colors">Home</a>
              <a href="#collection" className="text-gray-700 hover:text-gray-900 transition-colors">Collection</a>
              <a href="#about" className="text-gray-700 hover:text-gray-900 transition-colors">About</a>
              <a href="#contact" className="text-gray-700 hover:text-gray-900 transition-colors">Contact</a>
            </div>
          </div>
        </div>
      </nav>

      {/* Hero Section */}
      <section id="home" className="relative overflow-hidden">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-20 md:py-32">
          <div className="text-center">
            <h2 className="text-5xl md:text-7xl font-bold text-gray-900 mb-6 tracking-tight">
              Discover Your
              <span className="block bg-gradient-to-r from-blue-600 to-purple-600 bg-clip-text text-transparent">
                Perfect Style
              </span>
            </h2>
            <p className="text-xl md:text-2xl text-gray-600 mb-12 max-w-3xl mx-auto">
              Koleksi outfit pilihan untuk setiap momen dalam hidupmu. Dari casual hingga formal, temukan gaya yang sempurna.
            </p>
            <a 
              href="#collection" 
              className="inline-block bg-gray-900 text-white px-8 py-4 rounded-full text-lg font-semibold hover:bg-gray-800 transition-all transform hover:scale-105 shadow-lg"
            >
              Explore Collection
            </a>
          </div>
        </div>
        
        {/* Decorative Elements */}
        <div className="absolute top-20 left-10 w-72 h-72 bg-purple-200 rounded-full mix-blend-multiply filter blur-xl opacity-30 animate-blob"></div>
        <div className="absolute top-40 right-10 w-72 h-72 bg-blue-200 rounded-full mix-blend-multiply filter blur-xl opacity-30 animate-blob animation-delay-2000"></div>
        <div className="absolute -bottom-8 left-1/2 w-72 h-72 bg-pink-200 rounded-full mix-blend-multiply filter blur-xl opacity-30 animate-blob animation-delay-4000"></div>
      </section>

      {/* Collection Section */}
      <section id="collection" className="py-20 bg-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="text-center mb-16">
            <h3 className="text-4xl md:text-5xl font-bold text-gray-900 mb-4">
              Outfit Collection
            </h3>
            <p className="text-lg text-gray-600 max-w-2xl mx-auto">
              Pilih kategori favoritmu dan temukan inspirasi outfit yang sesuai dengan gaya hidupmu
            </p>
          </div>

          {/* Category Filter */}
          <div className="flex flex-wrap justify-center gap-3 mb-12">
            {categories.map((category) => (
              <button
                key={category}
                onClick={() => setSelectedCategory(category)}
                className={`px-6 py-3 rounded-full font-semibold transition-all transform hover:scale-105 ${
                  selectedCategory === category
                    ? "bg-gray-900 text-white shadow-lg"
                    : "bg-gray-100 text-gray-700 hover:bg-gray-200"
                }`}
              >
                {category}
              </button>
            ))}
          </div>

          {/* Outfit Grid */}
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
            {filteredOutfits.map((outfit) => (
              <Link href={`/outfit/${outfit.id}`} key={outfit.id}>
                <div className="group cursor-pointer">
                  <div className={`${outfit.color} rounded-3xl p-8 h-80 flex flex-col justify-between transition-all transform group-hover:scale-105 group-hover:shadow-2xl`}>
                    <div>
                      <div className="flex justify-between items-start mb-4">
                        <span className="px-4 py-1 bg-white/80 backdrop-blur-sm rounded-full text-sm font-semibold text-gray-700">
                          {outfit.category}
                        </span>
                        <span className="text-2xl">✨</span>
                      </div>
                      <h4 className="text-2xl font-bold text-gray-900 mb-2">
                        {outfit.name}
                      </h4>
                      <p className="text-gray-600 mb-4">
                        {outfit.description}
                      </p>
                    </div>
                    <div>
                      <div className="flex flex-wrap gap-2 mb-3">
                        {outfit.items.slice(0, 3).map((item, idx) => (
                          <span key={idx} className="px-3 py-1 bg-white/60 backdrop-blur-sm rounded-full text-xs font-medium text-gray-700">
                            {item}
                          </span>
                        ))}
                      </div>
                      <div className="flex justify-between items-center text-sm text-gray-600">
                        <span>{outfit.season}</span>
                        <span className="font-semibold text-gray-900 group-hover:translate-x-1 transition-transform">
                          View Details →
                        </span>
                      </div>
                    </div>
                  </div>
                </div>
              </Link>
            ))}
          </div>
        </div>
      </section>

      {/* About Section */}
      <section id="about" className="py-20 bg-gradient-to-br from-gray-50 to-gray-100">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="grid md:grid-cols-2 gap-12 items-center">
            <div>
              <h3 className="text-4xl font-bold text-gray-900 mb-6">
                Tentang OutfitHub
              </h3>
              <p className="text-lg text-gray-600 mb-6">
                OutfitHub adalah platform inspirasi fashion yang membantu kamu menemukan gaya berpakaian yang sempurna untuk setiap kesempatan. Kami percaya bahwa setiap orang memiliki gaya unik mereka sendiri.
              </p>
              <p className="text-lg text-gray-600 mb-8">
                Dari outfit casual untuk hangout bersama teman, hingga formal wear untuk acara penting, kami menyediakan inspirasi yang kamu butuhkan untuk tampil percaya diri setiap hari.
              </p>
              <div className="grid grid-cols-3 gap-6">
                <div className="text-center">
                  <div className="text-3xl font-bold text-gray-900 mb-2">100+</div>
                  <div className="text-sm text-gray-600">Outfit Ideas</div>
                </div>
                <div className="text-center">
                  <div className="text-3xl font-bold text-gray-900 mb-2">50K+</div>
                  <div className="text-sm text-gray-600">Happy Users</div>
                </div>
                <div className="text-center">
                  <div className="text-3xl font-bold text-gray-900 mb-2">24/7</div>
                  <div className="text-sm text-gray-600">Inspiration</div>
                </div>
              </div>
            </div>
            <div className="bg-gradient-to-br from-blue-100 to-purple-100 rounded-3xl h-96 flex items-center justify-center">
              <div className="text-center">
                <div className="text-8xl mb-4">👔</div>
                <p className="text-xl font-semibold text-gray-700">Style Inspiration</p>
              </div>
            </div>
          </div>
        </div>
      </section>

      {/* Footer */}
      <footer id="contact" className="bg-gray-900 text-white py-12">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="grid md:grid-cols-4 gap-8 mb-8">
            <div>
              <h4 className="text-xl font-bold mb-4">OutfitHub</h4>
              <p className="text-gray-400">
                Your daily fashion inspiration platform
              </p>
            </div>
            <div>
              <h5 className="font-semibold mb-4">Quick Links</h5>
              <ul className="space-y-2 text-gray-400">
                <li><a href="#home" className="hover:text-white transition-colors">Home</a></li>
                <li><a href="#collection" className="hover:text-white transition-colors">Collection</a></li>
                <li><a href="#about" className="hover:text-white transition-colors">About</a></li>
              </ul>
            </div>
            <div>
              <h5 className="font-semibold mb-4">Categories</h5>
              <ul className="space-y-2 text-gray-400">
                <li>Casual</li>
                <li>Formal</li>
                <li>Urban</li>
                <li>Athletic</li>
              </ul>
            </div>
            <div>
              <h5 className="font-semibold mb-4">Connect</h5>
              <ul className="space-y-2 text-gray-400">
                <li>Instagram</li>
                <li>Twitter</li>
                <li>Pinterest</li>
                <li>TikTok</li>
              </ul>
            </div>
          </div>
          <div className="border-t border-gray-800 pt-8 text-center text-gray-400">
            <p>&copy; 2025 OutfitHub. All rights reserved.</p>
          </div>
        </div>
      </footer>
    </div>
  );
}
