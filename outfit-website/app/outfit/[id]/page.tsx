"use client";

import { useParams } from "next/navigation";
import Link from "next/link";

const outfitsData = [
  {
    id: 1,
    name: "Summer Breeze",
    category: "Casual",
    description: "Perfect untuk hangout santai di akhir pekan",
    fullDescription: "Outfit ini menggabungkan kenyamanan dan gaya untuk tampilan kasual yang sempurna. Cocok untuk cuaca hangat dan aktivitas outdoor yang santai.",
    items: [
      { name: "White Linen Shirt", description: "Kemeja linen putih yang breathable dan nyaman" },
      { name: "Light Blue Jeans", description: "Jeans warna biru muda dengan fit yang sempurna" },
      { name: "White Sneakers", description: "Sneakers putih klasik yang versatile" }
    ],
    season: "Summer",
    occasion: "Casual Outing",
    color: "bg-gradient-to-br from-blue-50 to-cyan-50",
    tips: [
      "Pilih linen untuk material yang breathable",
      "Kombinasi warna netral mudah dipadukan",
      "Tambahkan aksesori minimal untuk tampilan clean"
    ],
    styleNotes: "Gaya ini menekankan kesederhanaan dan kenyamanan. Warna-warna terang mencerminkan vibes musim panas yang ceria."
  },
  {
    id: 2,
    name: "Business Chic",
    category: "Formal",
    description: "Tampil profesional dan percaya diri di kantor",
    fullDescription: "Outfit formal yang sempurna untuk lingkungan kerja profesional. Memberikan kesan kompeten dan dapat dipercaya.",
    items: [
      { name: "Navy Blazer", description: "Blazer navy yang tailored dengan sempurna" },
      { name: "White Dress Shirt", description: "Kemeja dress putih crisp dan formal" },
      { name: "Black Trousers", description: "Celana panjang hitam dengan potongan slim fit" },
      { name: "Oxford Shoes", description: "Sepatu oxford kulit hitam yang elegan" }
    ],
    season: "All Season",
    occasion: "Office/Meeting",
    color: "bg-gradient-to-br from-slate-50 to-gray-100",
    tips: [
      "Pastikan blazer fit dengan sempurna di bahu",
      "Kemeja harus selalu rapi dan di-iron",
      "Sepatu harus selalu bersih dan mengkilap"
    ],
    styleNotes: "Kombinasi klasik yang tidak pernah salah. Navy dan hitam menciptakan tampilan yang powerful dan profesional."
  },
  {
    id: 3,
    name: "Street Style",
    category: "Urban",
    description: "Gaya urban yang edgy dan modern",
    fullDescription: "Outfit streetwear yang mencerminkan gaya hidup urban modern. Kombinasi comfort dan style yang edgy.",
    items: [
      { name: "Black Hoodie", description: "Hoodie hitam oversized dengan material premium" },
      { name: "Cargo Pants", description: "Celana cargo dengan banyak pocket dan utility" },
      { name: "High-top Sneakers", description: "Sneakers high-top dengan desain bold" },
      { name: "Baseball Cap", description: "Topi baseball untuk melengkapi look" }
    ],
    season: "Fall/Winter",
    occasion: "Streetwear",
    color: "bg-gradient-to-br from-zinc-50 to-stone-100",
    tips: [
      "Layering adalah kunci untuk streetwear",
      "Jangan takut mix and match textures",
      "Aksesori seperti topi dan tas menambah character"
    ],
    styleNotes: "Street style adalah tentang ekspresikan diri. Warna gelap dan siluet oversized menciptakan aesthetic yang kuat."
  },
  {
    id: 4,
    name: "Elegant Evening",
    category: "Formal",
    description: "Sempurna untuk acara malam yang elegan",
    fullDescription: "Outfit formal premium untuk acara-acara special di malam hari. Memberikan kesan sophisticated dan elegant.",
    items: [
      { name: "Black Suit", description: "Suit hitam dengan tailoring sempurna" },
      { name: "Silk Tie", description: "Dasi sutra dengan pattern subtle" },
      { name: "Leather Dress Shoes", description: "Sepatu kulit formal dengan finishing premium" },
      { name: "Watch", description: "Jam tangan klasik sebagai statement piece" }
    ],
    season: "All Season",
    occasion: "Evening Event",
    color: "bg-gradient-to-br from-purple-50 to-pink-50",
    tips: [
      "Suit harus custom fit untuk hasil terbaik",
      "Pilih aksesori yang berkualitas tinggi",
      "Perhatikan detail seperti cufflinks dan pocket square"
    ],
    styleNotes: "Black tie elegance yang timeless. Setiap detail harus sempurna untuk menciptakan kesan yang memorable."
  },
  {
    id: 5,
    name: "Beach Vibes",
    category: "Casual",
    description: "Santai dan nyaman untuk liburan pantai",
    fullDescription: "Outfit liburan yang perfect untuk aktivitas pantai. Kombinasi style dan functionality untuk cuaca panas.",
    items: [
      { name: "Floral Shirt", description: "Kemeja floral dengan pattern tropical" },
      { name: "Khaki Shorts", description: "Celana pendek khaki yang comfortable" },
      { name: "Sandals", description: "Sandal yang nyaman untuk berjalan di pantai" },
      { name: "Sunglasses", description: "Kacamata hitam untuk proteksi dan style" }
    ],
    season: "Summer",
    occasion: "Beach/Vacation",
    color: "bg-gradient-to-br from-orange-50 to-amber-50",
    tips: [
      "Pilih material yang quick-dry",
      "Warna cerah cocok untuk suasana pantai",
      "Jangan lupa sunscreen dan topi"
    ],
    styleNotes: "Vacation mode dengan pattern fun dan warna-warna cerah. Comfort adalah prioritas tanpa mengorbankan style."
  },
  {
    id: 6,
    name: "Sporty Active",
    category: "Athletic",
    description: "Outfit olahraga yang stylish dan fungsional",
    fullDescription: "Activewear yang menggabungkan performance dan style. Perfect untuk gym, running, atau aktivitas olahraga lainnya.",
    items: [
      { name: "Track Jacket", description: "Jaket track dengan material moisture-wicking" },
      { name: "Athletic Pants", description: "Celana olahraga dengan fit yang comfortable" },
      { name: "Running Shoes", description: "Sepatu running dengan cushioning optimal" },
      { name: "Sports Watch", description: "Smartwatch untuk tracking aktivitas" }
    ],
    season: "All Season",
    occasion: "Sports/Gym",
    color: "bg-gradient-to-br from-green-50 to-emerald-50",
    tips: [
      "Pilih material yang breathable dan stretchy",
      "Fit harus memungkinkan range of motion penuh",
      "Investasi di sepatu yang berkualitas untuk proteksi"
    ],
    styleNotes: "Athleisure yang functional dan fashionable. Material technical dan design modern untuk performance optimal."
  }
];

export default function OutfitDetail() {
  const params = useParams();
  const id = parseInt(params.id as string);
  const outfit = outfitsData.find(o => o.id === id);

  if (!outfit) {
    return (
      <div className="min-h-screen flex items-center justify-center">
        <div className="text-center">
          <h1 className="text-4xl font-bold text-gray-900 mb-4">Outfit Not Found</h1>
          <Link href="/" className="text-blue-600 hover:underline">
            Back to Home
          </Link>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-gray-50 via-white to-gray-50">
      {/* Navigation */}
      <nav className="border-b border-gray-200 bg-white/80 backdrop-blur-md sticky top-0 z-50">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex justify-between items-center h-16">
            <Link href="/" className="flex items-center">
              <h1 className="text-2xl font-bold bg-gradient-to-r from-gray-900 to-gray-600 bg-clip-text text-transparent">
                OutfitHub
              </h1>
            </Link>
            <Link 
              href="/" 
              className="text-gray-700 hover:text-gray-900 transition-colors font-medium"
            >
              ← Back to Collection
            </Link>
          </div>
        </div>
      </nav>

      {/* Hero Section */}
      <section className={`${outfit.color} py-20`}>
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="text-center">
            <span className="inline-block px-4 py-2 bg-white/80 backdrop-blur-sm rounded-full text-sm font-semibold text-gray-700 mb-4">
              {outfit.category}
            </span>
            <h1 className="text-5xl md:text-6xl font-bold text-gray-900 mb-6">
              {outfit.name}
            </h1>
            <p className="text-xl text-gray-600 max-w-3xl mx-auto mb-8">
              {outfit.fullDescription}
            </p>
            <div className="flex flex-wrap justify-center gap-4 text-sm">
              <span className="px-4 py-2 bg-white/60 backdrop-blur-sm rounded-full font-medium text-gray-700">
                Season: {outfit.season}
              </span>
              <span className="px-4 py-2 bg-white/60 backdrop-blur-sm rounded-full font-medium text-gray-700">
                Occasion: {outfit.occasion}
              </span>
            </div>
          </div>
        </div>
      </section>

      {/* Outfit Items */}
      <section className="py-20 bg-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <h2 className="text-4xl font-bold text-gray-900 mb-12 text-center">
            Outfit Components
          </h2>
          <div className="grid md:grid-cols-2 gap-8">
            {outfit.items.map((item, index) => (
              <div 
                key={index}
                className="bg-gradient-to-br from-gray-50 to-white rounded-2xl p-8 border border-gray-200 hover:shadow-xl transition-all transform hover:scale-105"
              >
                <div className="flex items-start gap-4">
                  <div className="w-12 h-12 bg-gray-900 rounded-full flex items-center justify-center text-white font-bold text-xl flex-shrink-0">
                    {index + 1}
                  </div>
                  <div>
                    <h3 className="text-2xl font-bold text-gray-900 mb-2">
                      {item.name}
                    </h3>
                    <p className="text-gray-600">
                      {item.description}
                    </p>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </section>

      {/* Styling Tips */}
      <section className="py-20 bg-gradient-to-br from-gray-50 to-gray-100">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="grid md:grid-cols-2 gap-12">
            <div>
              <h2 className="text-4xl font-bold text-gray-900 mb-8">
                Styling Tips
              </h2>
              <ul className="space-y-4">
                {outfit.tips.map((tip, index) => (
                  <li key={index} className="flex items-start gap-4">
                    <span className="w-8 h-8 bg-gray-900 rounded-full flex items-center justify-center text-white font-bold flex-shrink-0">
                      ✓
                    </span>
                    <p className="text-lg text-gray-700 pt-1">{tip}</p>
                  </li>
                ))}
              </ul>
            </div>
            <div className="bg-white rounded-3xl p-8 shadow-lg">
              <h3 className="text-2xl font-bold text-gray-900 mb-4">
                Style Notes
              </h3>
              <p className="text-lg text-gray-600 leading-relaxed mb-6">
                {outfit.styleNotes}
              </p>
              <div className="border-t border-gray-200 pt-6">
                <h4 className="font-semibold text-gray-900 mb-3">Perfect For:</h4>
                <p className="text-gray-600">{outfit.occasion}</p>
              </div>
            </div>
          </div>
        </div>
      </section>

      {/* CTA Section */}
      <section className="py-20 bg-gray-900 text-white">
        <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 text-center">
          <h2 className="text-4xl font-bold mb-6">
            Ready to Try This Look?
          </h2>
          <p className="text-xl text-gray-300 mb-8">
            Explore lebih banyak outfit inspirations dan temukan gaya yang sempurna untukmu
          </p>
          <Link 
            href="/"
            className="inline-block bg-white text-gray-900 px-8 py-4 rounded-full text-lg font-semibold hover:bg-gray-100 transition-all transform hover:scale-105"
          >
            Browse More Outfits
          </Link>
        </div>
      </section>

      {/* Footer */}
      <footer className="bg-gray-900 text-white py-12 border-t border-gray-800">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 text-center text-gray-400">
          <p>&copy; 2025 OutfitHub. All rights reserved.</p>
        </div>
      </footer>
    </div>
  );
}
